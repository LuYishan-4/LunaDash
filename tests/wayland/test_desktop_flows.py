"""Real headless shell interactions; no physical GPU/login validation implied."""

import json
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import sys
import tempfile
import time

from PIL import Image, ImageDraw

if os.environ.get("CI", "").lower() != "true":
    raise SystemExit("This test starts an isolated compositor only in CI.")

build = Path(sys.argv[1]).resolve()
evidence = build / "ci-evidence/shell-layout/desktop-flows"
evidence.mkdir(parents=True, exist_ok=True)
create = re.compile(r'get_layer_surface\(new id zwlr_layer_surface_v1[@#](\d+),.*?,\s*\d+,\s*"([^"]+)"\)')
event = re.compile(r"zwlr_layer_surface_v1[@#](\d+)\.(configure|destroy)\(([^)]*)\)")
errors = re.compile(
    r"ReferenceError:|TypeError:|SyntaxError:|Cannot assign to non-existent property|"
    r"Unable to assign|Binding loop detected|"
    r"Detected anchors on an item that is managed by a layout|"
    r'Type \w+ unavailable|module "[^"]+" is not installed|'
    r"Failed to (?:load|create) (?:configuration|component)|"
    r"LunaDash shell exited; scheduling restart|LunaDash child exited abnormally",
    re.IGNORECASE,
)

with tempfile.TemporaryDirectory(prefix="lunadash-desktop-flows-") as temporary:
    runtime = Path(temporary)
    for directory in ("config", "cache", "data", "state"):
        (runtime / directory).mkdir()
    name = "lunadash-desktop-flows"
    control = runtime / (name + "-control")
    env = os.environ | {
        "XDG_RUNTIME_DIR": str(runtime),
        "XDG_CONFIG_HOME": str(runtime / "config"),
        "XDG_CACHE_HOME": str(runtime / "cache"),
        "XDG_DATA_HOME": str(runtime / "data"),
        "XDG_STATE_HOME": str(runtime / "state"),
        "WLR_BACKENDS": "headless", "WLR_HEADLESS_OUTPUTS": "1",
        "WLR_RENDERER": "pixman", "QT_QUICK_BACKEND": "software",
        "QSG_RHI_BACKEND": "software", "QT_FORCE_STDERR_LOGGING": "1",
        "LIBGL_ALWAYS_SOFTWARE": "1", "LUDASH_SKIP_SETUP": "1",
        "LUDASH_LANGUAGE": "en_US", "LUDASH_DISABLE_XWAYLAND": "1",
        "LUNADASH_DISABLE_FCITX": "1", "LUNADASH_QML_WATCH": "0",
        "WAYLAND_DEBUG": "client", "LC_ALL": "C.UTF-8",
        # The public desktop-size command is available only in nested sessions.
        "WAYLAND_DISPLAY": "lunadash-ci-headless-parent-marker",
    }
    for key in ("DISPLAY", "XAUTHORITY", "MESA_GL_VERSION_OVERRIDE",
                "MESA_GLSL_VERSION_OVERRIDE", "LUNADASH_CONTROL", "LUDASH_CONTROL",
                "LUNADASH_PUBLISH_ACTIVATION_ENV"):
        env.pop(key, None)
    client_env = env | {"WAYLAND_DISPLAY": name}
    client_env.pop("WAYLAND_DEBUG", None)
    log_path = evidence / "session.log"

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
            result = json.loads(data)
            assert "error" not in result, result
            return result

    def layer(namespace):
        live = {}
        for line in log_path.read_text(errors="replace").splitlines():
            created = create.search(line)
            if created:
                live[created[1]] = {"namespace": created[2]}
            updated = event.search(line)
            if updated and updated[1] in live:
                if updated[2] == "destroy":
                    del live[updated[1]]
                else:
                    live[updated[1]]["size"] = [int(value.strip()) for value in updated[3].split(",")][-2:]
        matches = [item for item in live.values() if item["namespace"] == namespace and "size" in item]
        return matches[-1] if matches else None

    def key(*arguments):
        subprocess.run(["wtype", *arguments], env=client_env, check=True,
                       stdout=log, stderr=log, timeout=8)
        time.sleep(0.15)

    def tabs(count):
        key(*[argument for _ in range(count) for argument in ("-k", "Tab")])

    def capture(label, size=(1920, 1080)):
        path = evidence / (label + ".png")
        subprocess.run(["grim", "-s", "1", str(path)], env=client_env,
                       check=True, stdout=log, stderr=log, timeout=8)
        with Image.open(path) as image:
            assert image.size == size, (label, image.size, size)
            assert image.convert("RGB").getcolors(maxcolors=256) is None, "Blank screenshot"

    def wallpaper_action(*arguments):
        result = subprocess.run([str(build / "lunadash-shell-tool"), "wallpaper", *arguments],
                                env=env, capture_output=True, text=True, timeout=5)
        assert result.returncode == 0, result.stderr + result.stdout
        assert json.loads(result.stdout).get("ok"), result.stdout

    with log_path.open("w+") as log:
        process = subprocess.Popen(
            [str(build / "lunadash-compositor"), "--socket", name, "--exit-after", "110000"],
            env=env, stdout=log, stderr=log, start_new_session=True,
        )
        keeper = None

        def wait(predicate, description, timeout=15):
            deadline = time.monotonic() + timeout
            last = {}
            while time.monotonic() < deadline:
                assert process.poll() is None, f"Compositor exited: {description}"
                try:
                    last = request()
                    assert not last.get("processFailure"), last
                    if predicate(last):
                        return last
                except (OSError, ValueError):
                    pass
                time.sleep(0.15)
            raise AssertionError(f"Timed out: {description}; state={last}")

        try:
            wait(lambda state: state.get("layerSurfaces") == 3
                 and layer("lunadash-panel") and not layer("lunadash-startup"), "shell startup", 25)
            # Keep seat keyboard capabilities alive between short-lived wtype clients.
            keeper = subprocess.Popen(["wtype", "-s", "110000"], env=client_env, stdout=log, stderr=log)
            time.sleep(0.3)
            assert keeper.poll() is None
            request("appearance", json.dumps({"animations": True, "overview": False}))
            request("desktop-size", "1920x1080")
            wait(lambda state: state["display"]["width"] == 1920, "desktop resize")
            request("open-settings", "about")
            wait(lambda state: layer("lunadash-settings"), "About settings")
            time.sleep(0.6)
            capture("about")
            key("-M", "ctrl", "-k", "f", "-m", "ctrl")
            tabs(1)
            key("-k", "Return")
            wait(lambda state: (layer("lunadash-settings") or {}).get("size", [0])[0] > 1800,
                 "maximize through header control")
            time.sleep(0.4)
            capture("about-maximized")
            key("-M", "ctrl", "-k", "f", "-m", "ctrl")
            tabs(1)
            key("-k", "Return")
            wait(lambda state: 0 < (layer("lunadash-settings") or {}).get("size", [0])[0] < 1500,
                 "restore through header control")
            request("desktop-size", "800x600")
            wait(lambda state: state["display"]["width"] == 800, "compact output")
            time.sleep(0.6)
            compact = layer("lunadash-settings")
            assert compact and compact["size"][0] <= 800 and compact["size"][1] <= 600, compact
            capture("about-compact", (800, 600))
            key("-M", "ctrl", "-k", "f", "-m", "ctrl")
            tabs(2)
            key("-k", "Return")
            wait(lambda state: not layer("lunadash-settings"), "animated close finishes before unloading")
            request("desktop-size", "1920x1080")
            wait(lambda state: state["display"]["width"] == 1920, "gallery output")
            wallpapers = runtime / "wallpaper gallery"
            wallpapers.mkdir()
            for index in range(8):
                image = Image.new("RGB", (320, 180), (30 + index * 20, 70, 140))
                ImageDraw.Draw(image).ellipse((30, 25, 150, 150), fill=(110, 170, 220))
                image.save(wallpapers / f"garden {index}.png")
            request("appearance", json.dumps({"wallpaperDirectory": str(wallpapers)}))
            wallpaper_action("refresh")
            # Use the same public action as Super+W, not a privileged virtual shortcut.
            request("choose-wallpaper")
            wait(lambda state: layer("lunadash-wallpaper-gallery"), "gallery maps")
            assert not layer("lunadash-settings"), "Wallpaper shortcut opened Settings"
            time.sleep(0.6)
            capture("wallpaper-gallery")
            key("garden 3", "-k", "Return")
            selected = str(wallpapers / "garden 3.png")
            wait(lambda state: state["wallpapers"]["current"]["path"] == selected,
                 "typing immediately filters; Enter applies without an Apply button")
            wallpaper_action("favorite", selected, "true")
            wait(lambda state: any(entry.get("favorite") for entry in state["wallpapers"]["library"]),
                 "favorite crosses process and settings boundaries")
            request("choose-wallpaper")
            wait(lambda state: not layer("lunadash-wallpaper-gallery"), "second shortcut dismisses gallery")
            request("appearance", json.dumps({"overview": True}))
            wait(lambda state: layer("lunadash-control-center"), "Control Center")
            time.sleep(0.4)
            tabs(4)
            key("-k", "Return")
            wait(lambda state: (layer("lunadash-control-center") or {}).get("size", [0])[0] >= 800,
                 "Display opens inline")
            assert not layer("lunadash-settings"), "Display redirected to Settings"
            time.sleep(0.5)
            capture("inline-display")
            key("-k", "Escape")
            time.sleep(0.3)
            tabs(10)
            key("-k", "Return")
            wait(lambda state: layer("lunadash-settings") and not layer("lunadash-control-center"),
                 "upper-right gear opens full Settings")
            (evidence / "final-state.json").write_text(json.dumps(request(), indent=2))
            failures = [line for line in log_path.read_text(errors="replace").splitlines() if errors.search(line)]
            assert not failures, "QML runtime failures:\n" + "\n".join(failures[-30:])
        finally:
            if keeper is not None and keeper.poll() is None:
                keeper.terminate()
                keeper.wait(timeout=3)
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGTERM)
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait(timeout=5)

print("Settings animations/bounds, inline controls and directory gallery passed with live screenshots.")
