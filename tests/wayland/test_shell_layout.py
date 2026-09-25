"""CI-only smoke test of the shipped shell in a real headless Wayland session.

The compositor uses pixman and Qt Quick uses its software renderer. Screenshots
are evidence of this software session, not physical GPU or login-session tests.
Run from the dedicated Arch workflow with Quickshell, grim and Pillow installed.
"""

import json
import os
from pathlib import Path
import re
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time

from PIL import Image, ImageChops

if os.environ.get("CI", "").lower() != "true":
    raise SystemExit("This smoke test runs only in CI; no local session is started.")

build = Path(sys.argv[1]).resolve()
evidence = build / "ci-evidence" / "shell-layout"
evidence.mkdir(parents=True, exist_ok=True)
for executable in ("quickshell", "grim"):
    assert shutil.which(executable), f"Required runtime tool is missing: {executable}"
for executable in ("lunadash-compositor", "lunadash-desktop", "lunadashctl", "lunadash-shell-tool"):
    assert (build / executable).is_file(), f"Required build target is missing: {executable}"

# Read actual layer-shell requests, including popup sizes configured by wlroots.
# libwayland versions use either # or @ before protocol object identifiers.
LAYER_CREATE = re.compile(
    r'get_layer_surface\(new id zwlr_layer_surface_v1[@#](\d+),.*?,\s*(\d+),\s*"([^"]+)"\)'
)
LAYER_EVENT = re.compile(
    r"zwlr_layer_surface_v1[@#](\d+)\.(set_anchor|set_margin|configure|destroy)\(([^)]*)\)"
)
QML_FAILURE = re.compile(
    r"ReferenceError:|TypeError:|SyntaxError:|Cannot assign to non-existent property|"
    r"Unable to assign|Binding loop detected|"
    r"Detected anchors on an item that is managed by a layout|"
    r'Type \w+ unavailable|module "[^"]+" is not installed|'
    r"Failed to (?:load|create) (?:configuration|component)|"
    r"LunaDash shell exited; scheduling restart|LunaDash child exited abnormally",
    re.IGNORECASE,
)


def layers_from_log(path):
    layers = {}
    for line in path.read_text(errors="replace").splitlines():
        created = LAYER_CREATE.search(line)
        if created:
            identifier, layer, name = created.groups()
            layers[identifier] = {"namespace": name, "layer": int(layer)}
        event = LAYER_EVENT.search(line)
        if not event or event[1] not in layers:
            continue
        surface = layers[event[1]]
        if event[2] == "destroy":
            surface["destroyed"] = True
        else:
            values = [int(value.strip()) for value in event[3].split(",")]
            surface[event[2]] = values
    return list(layers.values())


def live_layer(path, namespace):
    matches = [layer for layer in layers_from_log(path)
               if layer["namespace"] == namespace and not layer.get("destroyed")
               and "configure" in layer]
    return matches[-1] if matches else None


def reject_qml_errors(path):
    errors = [line for line in path.read_text(errors="replace").splitlines()
              if QML_FAILURE.search(line)]
    assert not errors, "Quickshell runtime errors:\n" + "\n".join(errors[-25:])


with tempfile.TemporaryDirectory(prefix="lunadash-shell-layout-") as temporary:
    runtime = Path(temporary)
    os.chmod(runtime, 0o700)
    for directory in ("config", "cache", "data", "state"):
        (runtime / directory).mkdir()
    # The Files welcome dialog is independently covered by the Files tests.
    # Use an initialized, isolated profile so each launch adds one layout tile.
    files_config = runtime / "config" / "LunaDash"
    files_config.mkdir()
    (files_config / "file-associations.json").write_text(json.dumps({
        "version": 1, "initialized": True, "askOnFirstOpen": True, "associations": {},
    }))
    socket_name = "lunadash-layout"
    control = runtime / (socket_name + "-control")
    env = os.environ | {
        "XDG_RUNTIME_DIR": str(runtime),
        "XDG_CONFIG_HOME": str(runtime / "config"),
        "XDG_CACHE_HOME": str(runtime / "cache"),
        "XDG_DATA_HOME": str(runtime / "data"),
        "XDG_STATE_HOME": str(runtime / "state"),
        "WLR_BACKENDS": "headless",
        "WLR_HEADLESS_OUTPUTS": "1",
        "WLR_RENDERER": "pixman",
        "QT_QUICK_BACKEND": "software",
        "QSG_RHI_BACKEND": "software",
        "QT_FORCE_STDERR_LOGGING": "1",
        "LIBGL_ALWAYS_SOFTWARE": "1",
        "LUDASH_SKIP_SETUP": "1",
        "LUDASH_LANGUAGE": "en_US",
        "LUDASH_DISABLE_XWAYLAND": "1",
        "LUNADASH_DISABLE_FCITX": "1",
        "LUNADASH_QML_WATCH": "0",
        "WAYLAND_DEBUG": "client",
        "LC_ALL": "C.UTF-8",
        # The existing desktop-size API is restricted to nested sessions. This
        # marker selects that policy; WLR_BACKENDS explicitly remains headless,
        # and createClientEnvironment supplies the real socket to Quickshell.
        "WAYLAND_DISPLAY": "lunadash-ci-headless-parent-marker",
    }
    for key in ("DISPLAY", "XAUTHORITY", "MESA_GL_VERSION_OVERRIDE", "MESA_GLSL_VERSION_OVERRIDE",
                "LUNADASH_CONTROL", "LUDASH_CONTROL", "LUNADASH_PUBLISH_ACTIVATION_ENV"):
        env.pop(key, None)

    def request(method="status", value=""):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(3)
            connection.connect(str(control))
            connection.sendall(json.dumps({"method": method, "value": str(value)}).encode() + b"\n")
            data = b""
            while b"\n" not in data:
                chunk = connection.recv(65536)
                assert chunk and len(data) < 4 * 1024 * 1024, "Invalid control response"
                data += chunk
            response = json.loads(data)
            assert "error" not in response, response
            return response

    log_path = evidence / "session.log"
    snapshots = []
    with log_path.open("w+") as log:
        process = subprocess.Popen(
            [str(build / "lunadash-compositor"), "--socket", socket_name, "--exit-after", "150000"],
            env=env, stdout=log, stderr=log, start_new_session=True,
        )

        def wait_for(predicate, description, timeout=15):
            deadline = time.monotonic() + timeout
            last_state = {}
            while time.monotonic() < deadline:
                assert process.poll() is None, f"Compositor exited while waiting for {description}"
                reject_qml_errors(log_path)
                try:
                    last_state = request()
                    assert not last_state.get("processFailure"), last_state
                    if predicate(last_state):
                        return last_state
                except (OSError, ValueError):
                    pass
                time.sleep(0.15)
            raise AssertionError(f"Timed out waiting for {description}: {last_state}")

        def capture(name, size):
            image_path = evidence / f"{name}.png"
            capture_env = env | {"WAYLAND_DISPLAY": socket_name}
            capture_env.pop("WAYLAND_DEBUG", None)
            result = subprocess.run(
                ["grim", "-s", "1", str(image_path)], env=capture_env,
                capture_output=True, text=True, timeout=8,
            )
            (evidence / f"{name}-capture.log").write_text(result.stdout + result.stderr)
            assert result.returncode == 0, result.stderr
            with Image.open(image_path) as image:
                assert image.size == size, (image.size, size)
                converted = image.convert("RGB")
                assert converted.getcolors(maxcolors=256) is None, "Captured desktop is blank"
                return converted

        try:
            initial = wait_for(
                lambda state: state.get("layerSurfaces") == 3
                    and all(live_layer(log_path, name) is not None for name in
                            ("lunadash-wallpaper", "lunadash-panel", "lunadash-window-frames"))
                    and live_layer(log_path, "lunadash-startup") is None,
                "wallpaper, panel and window frames after the startup splash", timeout=25,
            )
            assert initial["appearance"]["dockEnabled"] is False, initial["appearance"]
            request("appearance", json.dumps({"animations": False, "overview": False}))
            for width, height in ((1920, 1080), (1280, 720)):
                request("desktop-size", f"{width}x{height}")
                wait_for(
                    lambda state: state["display"]["width"] == width
                        and state["display"]["height"] == height
                        and state["layerSurfaces"] == 3,
                    f"desktop resize to {width}x{height}",
                )
                time.sleep(0.8)
                baseline = capture(f"desktop-{width}x{height}", (width, height))
                request("appearance", json.dumps({"overview": True}))
                opened = wait_for(
                    lambda state: state["appearance"]["overview"]
                        and state["layerSurfaces"] == 4
                        and live_layer(log_path, "lunadash-control-center") is not None,
                    "control-center popup mapping",
                )
                time.sleep(0.8)
                popup = live_layer(log_path, "lunadash-control-center")
                panel = live_layer(log_path, "lunadash-panel")
                assert popup["layer"] == 2, popup
                assert popup.get("set_anchor") == [9], popup  # Top | Right.
                assert panel.get("set_anchor") == [13], panel  # Top | Left | Right.
                popup_width, popup_height = popup["configure"][-2:]
                assert (popup_width, popup_height) == (600, 520), popup
                top, right, bottom, left = popup.get("set_margin", [0, 0, 0, 0])
                assert top >= panel["configure"][-1] and right >= 0, (popup, panel)
                box = (width - right - popup_width, top, width - right, top + popup_height)
                assert box[0] >= 0 and box[1] >= 0 and box[2] <= width and box[3] <= height, box
                rendered = capture(f"control-center-{width}x{height}", (width, height))
                changed = ImageChops.difference(baseline.crop(box), rendered.crop(box)).convert("L")
                changed_pixels = changed.point(lambda value: 255 if value > 12 else 0).histogram()[255]
                assert changed_pixels > popup_width * popup_height * 0.15, "Popup mapped without visible content"
                snapshots.append({"resolution": [width, height], "panel": panel,
                                  "controlCenter": popup, "popupBox": box, "state": opened})
                request("appearance", json.dumps({"overview": False}))
                wait_for(lambda state: state["layerSurfaces"] == 3, "control-center close")
            # Exercise the screenshot's recursive split using actual Qt Wayland
            # clients, then verify that the same clients receive real backdrops.
            request("desktop-size", "1920x1080")
            wait_for(lambda state: state["display"]["width"] == 1920, "large split-layout output")
            opened_ids = []
            main_geometry = None
            for index in range(6):
                if index == 3:
                    request("focus", opened_ids[1])
                request("launch-default", "files")
                state = wait_for(
                    lambda state: len([client for client in state["clients"]
                                      if client["mapped"] and not client.get("desktop")
                                      and not client.get("utility")]) == index + 1,
                    f"application {index + 1} mapping",
                )
                clients = [client for client in state["clients"] if client["mapped"]
                           and not client.get("desktop") and not client.get("utility")]
                added = [client for client in clients if client["id"] not in opened_ids]
                assert len(added) == 1, clients
                opened_ids.append(added[0]["id"])
                if index == 1:
                    main = next(client for client in clients if client["id"] == opened_ids[0])
                    main_geometry = [main[key] for key in ("x", "y", "width", "height")]
            glass = wait_for(lambda state: state.get("blurReady")
                             and not state.get("blurFailed") and state.get("blurFrames", 0) > 0,
                             "application backdrop rendering")
            main = next(client for client in glass["clients"] if client["id"] == opened_ids[0])
            assert [main[key] for key in ("x", "y", "width", "height")] == main_geometry, main
            assert main["x"] >= 900, main
            time.sleep(0.5)
            capture("recursive-windows-frosted-1920x1080", (1920, 1080))
            snapshots.append({"scene": "recursive-windows-frosted", "state": glass})
            request("appearance", json.dumps({"blur": False, "windowOpacity": 100}))
            wait_for(lambda state: not state.get("blurReady") and not state.get("blurFailed"),
                     "disabled backdrop rendering")
            capture("recursive-windows-opaque-1920x1080", (1920, 1080))
            request("appearance", json.dumps({"blur": True, "windowOpacity": 90}))
            wait_for(lambda state: state.get("blurReady") and not state.get("blurFailed"),
                     "restored backdrop rendering")
            assert not any(layer["namespace"] == "lunadash-dock" for layer in layers_from_log(log_path)), (
                "The disabled bottom dock created a layer-shell surface"
            )
            reject_qml_errors(log_path)
            print("Quickshell layout passed: top panel, bounded right popup, no dock, recursive client tiles, frosted glass, and six screenshots.")
        except BaseException:
            print(log_path.read_text(errors="replace")[-20000:], file=sys.stderr)
            raise
        finally:
            (evidence / "snapshots.json").write_text(json.dumps(snapshots, indent=2))
            (evidence / "layer-surfaces.json").write_text(json.dumps(layers_from_log(log_path), indent=2))
            # Quickshell and its helpers belong to this isolated process group;
            # also stop them if a compositor startup error left children alive.
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait(timeout=5)
