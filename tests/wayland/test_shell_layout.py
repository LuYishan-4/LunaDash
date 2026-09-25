"""CI-only smoke test of the shipped shell in a real headless Wayland session.

The compositor uses pixman and Qt Quick uses its software renderer. Screenshots
are evidence of this software session, not physical GPU or login-session tests.
Run from the dedicated Arch workflow with Quickshell, grim, wtype, AT-SPI and
Pillow installed. Settings edits use its public accessible controls and keyboard.
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

from PIL import Image, ImageChops, ImageDraw, ImageStat

if os.environ.get("CI", "").lower() != "true":
    raise SystemExit("This smoke test runs only in CI; no local session is started.")

# This synchronous polling client must query the live tree, rather than retain
# libatspi's initial empty child cache before the shell maps its windows. Qt's
# bridge exposes the standard shared bus, without the optional private bus API.
os.environ["ATSPI_NO_CACHE"] = "1"
os.environ["ATSPI_DISABLE_P2P"] = "1"
import pyatspi
from gi.repository import GLib

build = Path(sys.argv[1]).resolve()
evidence = build / "ci-evidence" / "shell-layout"
evidence.mkdir(parents=True, exist_ok=True)
for executable in ("quickshell", "dolphin", "kitty", "grim", "wtype", "wl-copy", "wl-paste"):
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


def accessibility_nodes():
    """Read the public accessibility tree, without a shell test/debug endpoint."""
    context = GLib.MainContext.default()
    while context.pending():
        context.iteration(False)
    pending = [(pyatspi.Registry.getDesktop(0), 0)]
    visited = 0
    while pending and visited < 6000:
        node, depth = pending.pop()
        visited += 1
        try:
            role = node.getRoleName()
            name = node.name or ""
            description = node.description or ""
            try:
                action = node.queryAction()
                actions = [action.getName(index) for index in range(action.nActions)]
            except NotImplementedError:
                actions = []
            yield node, {"name": name, "role": role, "description": description,
                         "actions": actions, "children": node.childCount, "depth": depth}
            if depth < 30:
                pending.extend((node.getChildAtIndex(index), depth + 1)
                               for index in range(node.childCount - 1, -1, -1))
        except Exception:
            # A popup or settings page may disappear during an accessibility read.
            continue


with tempfile.TemporaryDirectory(prefix="lunadash-shell-layout-") as temporary:
    runtime = Path(temporary)
    os.chmod(runtime, 0o700)
    for directory in ("config", "cache", "data", "state"):
        (runtime / directory).mkdir()
    # Use Dolphin's normal new-window path with an isolated, empty session.
    (runtime / "config" / "dolphinrc").write_text(
        "[General]\nRememberOpenedTabs=false\n")
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
        "QT_LINUX_ACCESSIBILITY_ALWAYS_ON": "1",
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
                "LUNADASH_CONTROL", "LUDASH_CONTROL", "LUNADASH_PUBLISH_ACTIVATION_ENV",
                "NO_AT_BRIDGE", "QT_ACCESSIBILITY"):
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
    ui_actions = []
    keyboard_only = False
    keyboard_picker_path = ""
    keyboard_keeper = None
    terminals = []
    with log_path.open("w+") as log:
        process = subprocess.Popen(
            [str(build / "lunadash-compositor"), "--socket", socket_name, "--exit-after", "220000"],
            env=env, stdout=log, stderr=log, start_new_session=True,
        )

        def wait_for(predicate, description, timeout=15):
            deadline = time.monotonic() + timeout
            last_state = {}
            while time.monotonic() < deadline:
                assert process.poll() is None, f"Compositor exited while waiting for {description}"
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

        def find_control(name, kind="action", description=None, timeout=12):
            deadline = time.monotonic() + timeout
            last_tree = []
            while time.monotonic() < deadline:
                assert process.poll() is None, "Compositor exited during settings interaction"
                last_tree = []
                for node, details in accessibility_nodes():
                    last_tree.append(details)
                    if details["name"] != name or (description and details["description"] != description):
                        continue
                    try:
                        states = node.getState()
                        if not states.contains(pyatspi.STATE_ENABLED) or not states.contains(pyatspi.STATE_SENSITIVE):
                            continue
                        if kind == "action" and not details["actions"]:
                            continue
                        if kind == "editable":
                            node.queryEditableText()
                        if kind == "combo" and "combo" not in details["role"]:
                            continue
                        return node, details
                    except NotImplementedError:
                        continue
                time.sleep(0.2)
            (evidence / "accessibility-missing-control.json").write_text(json.dumps(last_tree, indent=2))
            raise AssertionError(f"Accessible Settings control is missing: {name} ({kind})")

        def press_control(name, description=None):
            control = accessible_control(name, description=description)
            if control is None:
                keyboard_press(name, description)
                return
            node, details = control
            try:
                details["scrolledIntoView"] = bool(node.queryComponent().scrollTo(pyatspi.SCROLL_ANYWHERE))
            except Exception:
                # AT-SPI actions remain available for controls inside a clipped
                # Settings page even when Qt does not implement ScrollTo.
                details["scrolledIntoView"] = False
            action = node.queryAction()
            preferred = next((index for index, label in enumerate(details["actions"])
                              if label.lower() in ("press", "click", "toggle")), 0)
            assert action.doAction(preferred), details
            ui_actions.append(details | {"operation": "activate", "action": details["actions"][preferred]})

        def key_input(*arguments):
            key_env = env | {"WAYLAND_DISPLAY": socket_name}
            key_env.pop("WAYLAND_DEBUG", None)
            completed = subprocess.run(["wtype", *arguments], env=key_env, text=True,
                                       capture_output=True, timeout=5)
            assert completed.returncode == 0, completed.stderr

        def edit_control(name, value):
            global keyboard_picker_path
            control = accessible_control(name, kind="editable")
            if control is None:
                keyboard_focus_field(name)
                details = {"name": name, "input": "Wayland keyboard"}
            else:
                node, details = control
                assert node.queryComponent().grabFocus(), details
            key_input("-M", "ctrl", "-k", "a", "-m", "ctrl", str(value), "-k", "Return")
            if name == "Image path":
                keyboard_picker_path = str(value)
            ui_actions.append(details | {"operation": "keyboard-edit", "value": value})

        def select_control(name, index):
            control = accessible_control(name, kind="combo")
            if control is None:
                assert name == "Panel length", name
                keyboard_margin_anchor()
                keyboard_tabs(3 if panel_document(request())["style"]["width"] > 0 else 2, backward=True)
                details = {"name": name, "input": "Wayland keyboard"}
            else:
                node, details = control
                assert node.queryComponent().grabFocus(), details
            key_input("-k", "space")
            # The shipped combo deliberately arms its popup after 140 ms to avoid
            # opening presses accidentally activating a choice.
            time.sleep(0.2)
            arguments = ["-k", "Home"]
            for _ in range(index):
                arguments.extend(("-k", "Down"))
            arguments.extend(("-k", "Return"))
            key_input(*arguments)
            ui_actions.append(details | {"operation": "keyboard-select", "index": index})

        def accessible_control(name, kind="action", description=None):
            global keyboard_only
            if keyboard_only:
                return None
            try:
                return find_control(name, kind, description, timeout=2)
            except AssertionError:
                # Some shipped Qt/Quickshell combinations publish an application
                # with no accessible windows. Keep testing its actual controls
                # through their normal Tab/keyboard path in that environment.
                keyboard_only = True
                ui_actions.append({"operation": "keyboard-navigation-fallback",
                                   "reason": "Qt/Quickshell exposes no usable accessible control"})
                return None

        def keyboard_tabs(count, backward=False):
            arguments = ["-M", "shift"] if backward else []
            arguments += [item for _ in range(count) for item in ("-k", "Tab")]
            if backward:
                arguments += ["-m", "shift"]
            key_input(*arguments)

        def focused_text():
            clipboard_env = env | {"WAYLAND_DISPLAY": socket_name}
            clipboard_env.pop("WAYLAND_DEBUG", None)
            marker = "lunadash-ci-focus-probe"
            # wl-copy forks a selection owner which retains its standard file
            # descriptors. A PIPE would keep communicate() waiting for EOF
            # after the successful foreground process has already exited.
            copied = subprocess.run(["wl-copy", "--type", "text/plain", marker],
                                    env=clipboard_env, stdout=log, stderr=log, timeout=5)
            assert copied.returncode == 0, "Could not set the keyboard focus probe clipboard"
            key_input("-M", "ctrl", "-k", "a", "-k", "c", "-m", "ctrl")
            time.sleep(0.05)
            pasted = subprocess.run(["wl-paste", "--no-newline", "--type", "text/plain"],
                                    env=clipboard_env, text=True, capture_output=True, timeout=5)
            return pasted.stdout if pasted.returncode == 0 and pasted.stdout != marker else None

        def seek_keyboard_text(expected, label, limit=24):
            # A selected TextField can copy its current value. Buttons, switches
            # and non-editable combos leave our clipboard marker unchanged. This
            # anchors navigation to a real field instead of relying on the
            # window's current focus after an asynchronous Settings save.
            observed = []
            for index in range(limit):
                text = focused_text()
                observed.append(text)
                if text == expected:
                    if not any(action["operation"] == "keyboard-field-anchor" for action in ui_actions):
                        capture("keyboard-settings-field-focus", (1920, 1080))
                    ui_actions.append({"operation": "keyboard-field-anchor", "name": label,
                                       "text": text, "tabs": index})
                    return
                if text is not None and ("\n" in text or "\t" in text):
                    # A multiline editor accepts Tab as text. Do not mutate a
                    # different setting when the intended focus path is wrong.
                    break
                keyboard_tabs(1)
            (evidence / "keyboard-focus-values.json").write_text(json.dumps(observed, indent=2))
            raise AssertionError(f"Keyboard navigation could not find {label} with value {expected!r}")

        def keyboard_margin_anchor():
            # Settings exposes the normal Find shortcut to focus its search
            # field. Reacquire that fixed starting point after each async save,
            # then stay within the first panel controls instead of wrapping to
            # unrelated multiline configuration editors at the end of the page.
            key_input("-M", "ctrl", "-k", "f", "-m", "ctrl")
            time.sleep(0.05)
            margin = panel_document(request())["style"].get("margin", 10)
            seek_keyboard_text(str(margin), "Panel margin", limit=12)

        def keyboard_focus_field(name):
            if name == "Image path":
                seek_keyboard_text(keyboard_picker_path or env.get("HOME", "/"), name)
                return
            keyboard_margin_anchor()
            if name == "Panel height":
                keyboard_tabs(1, backward=True)
            elif name == "Panel width":
                keyboard_tabs(2, backward=True)
            else:
                assert name == "Panel margin", name

        def keyboard_press(name, description):
            if name == "Use image":
                seek_keyboard_text(keyboard_picker_path, "Image path")
                # ImagePicker's path is followed by Go, Cancel and Use image.
                keyboard_tabs(3)
            else:
                keyboard_margin_anchor()
                forward = {"Centered launcher": 1, "Show CPU and memory": 3,
                           "Choose launcher image": 9, "Restore LunaDash logo": 10}
                if name in forward:
                    keyboard_tabs(forward[name])
                else:
                    assert name == "×" and description == "Quick hide settings", (name, description)
                    custom = panel_document(request())["style"]["width"] > 0
                    # Above margin: height, optional width, length, edge, close.
                    keyboard_tabs(5 if custom else 4, backward=True)
            key_input("-k", "space" if name in ("Centered launcher", "Show CPU and memory") else "Return")
            ui_actions.append({"operation": "keyboard-activate", "name": name})

        def assert_control_text(name, expected):
            control = accessible_control(name, kind="editable")
            if control is None:
                keyboard_focus_field(name)
                actual = focused_text()
            else:
                actual = control[0].queryText().getText(0, -1)
            assert actual == expected, (name, actual, expected)

        def panel_document(state):
            return state["shellModules"]["document"]["modules"]["panel"]

        def capture_workspace_pills(name, count, active):
            # Module config uses the shell's 1.2 s status poll; workspace focus
            # itself arrives sooner over the separate interaction channel.
            time.sleep(1.6)
            screenshot = capture(name, (1920, 1080)).convert("L")
            layer = live_layer(log_path, "lunadash-panel")
            top, _, _, left = layer["set_margin"]
            center_y = top + layer["configure"][-1] // 2
            # Inset row + list padding; these are the normal shipped panel metrics.
            x = left + 4 + 8
            for index in range(count):
                width = 34 if index == active else 16
                center_x = x + width // 2
                middle = screenshot.getpixel((center_x, center_y))
                above = screenshot.getpixel((center_x, center_y - 7))
                below = screenshot.getpixel((center_x, center_y + 7))
                assert middle > max(above, below) + 12, (
                    "Workspace pill missing or vertically displaced", name, index,
                    center_x, center_y, middle, above, below)
                x += width + 4

        def capture_palette_change(name, previous, brighten):
            # An external IPC acknowledgement is not a presented QML frame.
            # Keep the pixel contract, allowing the normal shell status poll
            # to deliver the palette before capturing its evidence.
            deadline = time.monotonic() + 8
            previous_crop = previous.crop(panel_box).convert("L")
            threshold = previous_crop.width * previous_crop.height * 0.08
            changed = 0
            while time.monotonic() < deadline:
                current = capture(name, (1920, 1080))
                crop = current.crop(panel_box).convert("L")
                difference = ImageChops.subtract(crop, previous_crop) if brighten else ImageChops.subtract(previous_crop, crop)
                changed = difference.point(lambda value: 255 if value > 12 else 0).histogram()[255]
                if changed > threshold:
                    return current, changed
                time.sleep(0.35)
            raise AssertionError(f"Panel palette did not render: {name}, {changed} changed pixels; expected > {threshold}")

        def application_clients(state):
            return [client for client in state["clients"] if client["mapped"]
                    and not client.get("desktop") and not client.get("utility")]

        def clients_settled(state, count):
            clients = application_clients(state)
            return len(clients) == count and all(
                client["contentGeometryWidth"] == client["width"]
                and client["contentGeometryHeight"] == client["height"]
                and client["frameX"] == client["x"] and client["frameY"] == client["y"]
                and client["frameWidth"] == client["width"]
                and client["frameHeight"] == client["height"]
                and client["cornerRadius"] == 16
                for client in clients)

        def verify_tiles(state):
            area = state["workArea"]
            clients = application_clients(state)
            assert area["y"] >= panel_box[3], (area, panel_box)
            boxes = []
            for client in clients:
                assert client["visible"] and client["contentVisible"], client
                assert client["width"] >= 320 and client["height"] >= 300, client
                x, y, width, height = (client[key] for key in ("frameX", "frameY", "frameWidth", "frameHeight"))
                box = (x, y, x + width, y + height)
                assert x >= area["x"] and y >= area["y"], (client, area)
                assert box[2] <= area["x"] + area["width"], (client, area)
                assert box[3] <= area["y"] + area["height"], (client, area)
                for previous in boxes:
                    assert box[2] <= previous[0] or previous[2] <= box[0] or box[3] <= previous[1] or previous[3] <= box[1], (boxes, box)
                boxes.append(box)
            return clients

        def verify_corners(name, rendered, baseline, clients):
            # The contrasting CI wallpaper makes a painted square corner visibly
            # different from the expected uncovered desktop in both glass modes.
            atlas = Image.new("RGB", (4 * 256, len(clients) * 280), "#111111")
            labels = ImageDraw.Draw(atlas)
            metrics = []
            errors = []
            for row, client in enumerate(clients):
                x, y, width, height = (client[key] for key in ("frameX", "frameY", "frameWidth", "frameHeight"))
                radius = client["cornerRadius"]
                corners = (("top-left", x, y, 1, 1),
                           ("top-right", x + width - 1, y, -1, 1),
                           ("bottom-left", x, y + height - 1, 1, -1),
                           ("bottom-right", x + width - 1, y + height - 1, -1, -1))
                for column, (label, cx, cy, dx, dy) in enumerate(corners):
                    px, py = min(cx, cx + dx), min(cy, cy + dy)
                    patch = (px, py, px + 2, py + 2)
                    outside = ImageStat.Stat(ImageChops.difference(rendered.crop(patch), baseline.crop(patch))).mean
                    ix, iy = cx + dx * (radius + 6), cy + dy * (radius + 6)
                    inner_patch = (ix, iy, ix + 2, iy + 2)
                    inside = ImageStat.Stat(ImageChops.difference(rendered.crop(inner_patch), baseline.crop(inner_patch))).mean
                    metrics.append({"client": client["id"], "corner": label,
                                    "desktopDifference": outside, "contentDifference": inside})
                    if max(outside) > 8:
                        errors.append((client["id"], label, "square corner painted over desktop", outside))
                    if max(inside) < 12:
                        errors.append((client["id"], label, "window interior did not paint", inside))
                    crop_x = cx - 4 if dx > 0 else cx - 27
                    crop_y = cy - 4 if dy > 0 else cy - 27
                    crop = rendered.crop((crop_x, crop_y, crop_x + 32, crop_y + 32))
                    atlas.paste(crop.resize((256, 256), Image.Resampling.NEAREST), (column * 256, row * 280))
                    labels.text((column * 256 + 4, row * 280 + 260), f"Window {client['id']} / {label}", fill="white")
            atlas.save(evidence / f"{name}-corner-details.png")
            (evidence / f"{name}-corner-pixels.json").write_text(json.dumps(metrics, indent=2))
            assert not errors, (name, errors)

        try:
            initial = wait_for(
                lambda state: state.get("layerSurfaces") == 3
                    and all(live_layer(log_path, name) is not None for name in
                            ("lunadash-wallpaper", "lunadash-panel", "lunadash-window-frames"))
                    and live_layer(log_path, "lunadash-startup") is None,
                "wallpaper, panel and window frames after the startup splash", timeout=25,
            )
            # A headless backend has no physical keyboard. Keep one standard
            # virtual keyboard connected while short-lived wtype commands send
            # input, so remapping a layer always has a keyboard for focus.
            keyboard_env = env | {"WAYLAND_DISPLAY": socket_name}
            keyboard_env.pop("WAYLAND_DEBUG", None)
            keyboard_keeper = subprocess.Popen(["wtype", "-s", "220000"], env=keyboard_env,
                                                stdout=log, stderr=log)
            time.sleep(0.2)
            assert keyboard_keeper.poll() is None, "The headless session's virtual keyboard did not start"
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
            # Capture native tiling and rounded pixels before UI automation so
            # an accessibility failure still leaves independent renderer evidence.
            request("desktop-size", "1920x1080")
            wait_for(lambda state: state["display"]["width"] == 1920, "large native rendering output")
            time.sleep(0.5)
            native_panel = live_layer(log_path, "lunadash-panel")
            top, right, bottom, left = native_panel.get("set_margin", [0, 0, 0, 0])
            panel_box = (left, top, 1920 - right, top + native_panel["configure"][-1])
            request("appearance", json.dumps({"themeMode": "dark"}))
            wait_for(lambda state: state["palette"]["dark"] is True, "native rendering dark palette")
            corner_wallpaper_path = evidence / "corner-check-wallpaper.png"
            corner_wallpaper = Image.new("RGB", (1920, 1080), "#2d598c")
            wallpaper_draw = ImageDraw.Draw(corner_wallpaper)
            for row in range(0, 1080, 24):
                for column in range(0, 1920, 24):
                    if (row // 24 + column // 24) % 2:
                        wallpaper_draw.rectangle((column, row, column + 23, row + 23), fill="#5e3778")
            corner_wallpaper.save(corner_wallpaper_path)
            request("wallpaper-image", str(corner_wallpaper_path))
            wait_for(lambda state: str(corner_wallpaper_path) in state["wallpaperImage"],
                     "contrasting wallpaper for native corner pixel checks")
            time.sleep(0.8)
            corner_baseline = capture("corner-reference-desktop-1920x1080", (1920, 1080))
            # A terminal child observes its PTY immediately, before any test
            # wait. Startup output must not be reflowed by a second size at map.
            # Dolphin exercises a different toolkit in the same generic layout.
            for count in (1, 3):
                report = evidence / f"terminal-startup-{count}.json"
                terminal = subprocess.Popen(
                    ["kitty", "--config", "NONE", "--title", f"Startup layout {count}",
                     "-o", "linux_display_server=wayland", "-o", "remember_window_size=no",
                     "-o", "initial_window_width=180c", "-o", "initial_window_height=50c",
                     "-o", "confirm_os_window_close=0", "-o", "font_size=11",
                     "python3", str(Path(__file__).with_name("terminal_startup_probe.py").resolve()),
                     str(report)], env=env | {"WAYLAND_DISPLAY": socket_name}, stdout=log, stderr=log,
                )
                terminals.append(terminal)
                startup_state = wait_for(lambda state: report.exists() and clients_settled(state, count),
                                         f"stable initial terminal size with {count} applications")
                sizes = json.loads(report.read_text())["sizes"]
                assert len(sizes) == 11 and sizes[0][0] > 0 and sizes[0][1] > 0, sizes
                assert all(size == sizes[0] for size in sizes), (
                    "Terminal PTY resized after drawing startup content", sizes)
                verify_tiles(startup_state)
                capture(f"terminal-startup-{count}-1920x1080", (1920, 1080))
                snapshots.append({"scene": f"terminal-startup-{count}", "ptySizes": sizes,
                                  "state": startup_state})
                if count == 1:
                    request("launch-default", "files")
                    mixed = wait_for(lambda state: clients_settled(state, 2), "Kitty and Dolphin tiles")
                    verify_tiles(mixed)
            for client in application_clients(startup_state):
                request("close", client["id"])
            wait_for(lambda state: not application_clients(state), "startup verification clients close")
            # Exercise the screenshot's recursive split using actual Qt Wayland
            # clients, then verify that the same clients receive real backdrops.
            request("desktop-size", "1920x1080")
            wait_for(lambda state: state["display"]["width"] == 1920, "large split-layout output")
            opened_ids = []
            for index in range(6):
                if index == 3:
                    request("focus", opened_ids[1])
                request("launch-default", "files")
                state = wait_for(
                    lambda state: clients_settled(state, index + 1),
                    f"application {index + 1} mapping with its configured buffer size",
                )
                clients = verify_tiles(state)
                added = [client for client in clients if client["id"] not in opened_ids]
                assert len(added) == 1, clients
                opened_ids.append(added[0]["id"])
                if index == 1:
                    main = next(client for client in clients if client["id"] == opened_ids[0])
                    assert main["x"] > added[0]["x"], clients
                snapshots.append({"scene": f"recursive-windows-{index + 1}", "state": state})
            glass = wait_for(lambda state: state.get("blurReady")
                             and not state.get("blurFailed") and state.get("blurFrames", 0) > 0,
                             "application backdrop rendering")
            clients = verify_tiles(glass)
            time.sleep(0.5)
            frosted = capture("recursive-windows-frosted-1920x1080", (1920, 1080))
            verify_corners("frosted", frosted, corner_baseline, clients)
            snapshots.append({"scene": "recursive-windows-frosted", "state": glass})
            request("appearance", json.dumps({"blur": False, "windowOpacity": 100}))
            opaque_state = wait_for(lambda state: not state.get("blurReady") and not state.get("blurFailed"),
                                   "disabled backdrop rendering")
            time.sleep(0.3)
            opaque = capture("recursive-windows-opaque-1920x1080", (1920, 1080))
            verify_corners("opaque", opaque, corner_baseline, verify_tiles(opaque_state))
            snapshots.append({"scene": "recursive-windows-opaque", "state": opaque_state})
            request("appearance", json.dumps({"blur": True, "windowOpacity": 90}))
            wait_for(lambda state: state.get("blurReady") and not state.get("blurFailed"),
                     "restored backdrop rendering")
            for client_id in opened_ids:
                request("close", client_id)
            wait_for(lambda state: not application_clients(state), "native verification clients close")
            # Reproduce the reported two -> three -> switch-back sequence with
            # the real author-style pill row and an external Dolphin window.
            saved_modules = request()["shellModules"]["document"]
            pill_modules = json.loads(json.dumps(saved_modules))
            pill_modules["modules"]["panel"]["config"].update({
                "workspacePills": True, "workspaceActiveWidth": 34,
                "workspaceInactiveWidth": 16, "workspacePillHeight": 8,
                "occupiedWorkspacesOnly": True,
            })
            assert "error" not in request("module-save", json.dumps(pill_modules))
            capture_workspace_pills("workspaces-initial-two", 2, 0)
            request("workspace", 1)
            wait_for(lambda state: state["workspace"] == 1, "visit empty second workspace")
            capture_workspace_pills("workspaces-empty-second", 2, 1)
            assert "error" not in request("launch-default", "files")
            workspace_state = wait_for(lambda state: clients_settled(state, 1), "Dolphin on workspace 2")
            workspace_client = application_clients(workspace_state)[0]
            assert "dolphin" in workspace_client["appId"].lower(), workspace_client
            capture_workspace_pills("workspaces-second-occupied", 3, 1)
            request("workspace", 0)
            wait_for(lambda state: state["workspace"] == 0, "return to first workspace")
            capture_workspace_pills("workspaces-third-retained", 3, 0)
            snapshots.append({"scene": "workspace-spare-retained", "state": request()})
            request("close", workspace_client["id"])
            wait_for(lambda state: not application_clients(state), "workspace test window closes")
            assert "error" not in request("module-save", json.dumps(saved_modules))
            request("wallpaper-default")
            wait_for(lambda state: str(corner_wallpaper_path) not in state["wallpaperImage"],
                     "restore the shipped wallpaper before Settings screenshots")
            time.sleep(0.8)
            # Settings values must pass through the shipped QML controls. IPC is
            # used to open the page and observe state, never to save these fields.
            request("desktop-size", "1920x1080")
            wait_for(lambda state: state["display"]["width"] == 1920, "large settings output")
            request("open-settings", "appearance")
            wait_for(lambda state: live_layer(log_path, "lunadash-settings") is not None,
                     "Appearance settings mapping")
            time.sleep(0.5)
            inherited_height = panel_document(request())["style"]["height"]
            press_control("Show CPU and memory")
            switched = wait_for(lambda state: panel_document(state)["config"]["showSystemStats"] is False,
                                "the actual Settings switch saves the panel config")
            assert panel_document(switched)["style"]["height"] == inherited_height, (
                "Keyboard navigation must not replace an inherited panel height"
            )
            press_control("Show CPU and memory")
            wait_for(lambda state: panel_document(state)["config"]["showSystemStats"] is True,
                     "the panel switch can be enabled again")
            press_control("Centered launcher")
            wait_for(lambda state: panel_document(state)["config"]["centerLauncher"] is False,
                     "the centered launcher switch saves")
            press_control("Centered launcher")
            wait_for(lambda state: panel_document(state)["config"]["centerLauncher"] is True,
                     "the centered launcher switch restores")
            select_control("Panel length", 1)
            wait_for(lambda state: panel_document(state)["style"]["width"] == 960,
                     "custom length saves before editing its width")
            edit_control("Panel width", "1200")
            wait_for(lambda state: panel_document(state)["style"]["width"] == 1200,
                     "custom panel width saves from its input field")
            edit_control("Panel height", "48")
            wait_for(lambda state: panel_document(state)["style"]["height"] == 48,
                     "panel height saves before editing its margin")
            edit_control("Panel margin", "14")
            configured = wait_for(
                lambda state: panel_document(state)["style"]["height"] == 48
                    and panel_document(state)["style"]["margin"] == 14,
                "panel height and margin save from Settings",
            )
            saved_path = runtime / "config" / "LuDash" / "shell-modules.json"
            saved = json.loads(saved_path.read_text())
            assert saved["modules"]["panel"]["style"]["width"] == 1200, saved
            assert saved["modules"]["panel"]["style"]["height"] == 48, saved
            assert saved["modules"]["panel"]["style"]["margin"] == 14, saved
            (evidence / "settings-saved-shell-modules.json").write_text(json.dumps(saved, indent=2))
            capture("settings-panel-saved-1920x1080", (1920, 1080))
            snapshots.append({"scene": "settings-ui-save", "state": configured})
            launcher_path = evidence / "custom-launcher.png"
            launcher = Image.new("RGBA", (64, 64), "#ed354f")
            launcher_draw = ImageDraw.Draw(launcher)
            launcher_draw.ellipse((12, 12, 52, 52), fill="#52f5ee")
            launcher.save(launcher_path)
            press_control("Choose launcher image")
            edit_control("Image path", str(launcher_path))
            capture("settings-launcher-image-picker-1920x1080", (1920, 1080))
            press_control("Use image")
            custom_logo_state = wait_for(
                lambda state: panel_document(state)["config"]["launcherImage"] == launcher_path.as_uri(),
                "the actual image picker saves the custom launcher image",
            )
            saved_logo = json.loads(saved_path.read_text())
            assert saved_logo["modules"]["panel"]["config"]["launcherImage"] == launcher_path.as_uri(), saved_logo
            (evidence / "settings-custom-launcher-modules.json").write_text(json.dumps(saved_logo, indent=2))
            snapshots.append({"scene": "settings-custom-launcher", "state": custom_logo_state})
            # Close and reopen the real page before checking the saved value.
            press_control("×", description="Quick hide settings")
            wait_for(lambda state: live_layer(log_path, "lunadash-settings") is None,
                     "Settings closes through its own button")
            time.sleep(0.4)
            custom_logo_image = capture("custom-panel-launcher-1920x1080", (1920, 1080))
            request("open-settings", "appearance")
            wait_for(lambda state: live_layer(log_path, "lunadash-settings") is not None,
                     "Settings reopens with the saved profile")
            assert_control_text("Panel width", "1200")
            press_control("Restore LunaDash logo")
            wait_for(lambda state: panel_document(state)["config"]["launcherImage"] == "",
                     "the Settings restore button reinstates the built-in LunaDash logo")
            restored_profile = json.loads(saved_path.read_text())
            assert restored_profile["modules"]["panel"]["config"]["launcherImage"] == "", restored_profile
            press_control("×", description="Quick hide settings")
            wait_for(lambda state: live_layer(log_path, "lunadash-settings") is None,
                     "Settings closes before panel rendering checks")
            wait_for(lambda state: live_layer(log_path, "lunadash-panel")["configure"][-2:] == [1200, 48],
                     "the Wayland panel uses its saved custom size")
            custom_panel = live_layer(log_path, "lunadash-panel")
            assert custom_panel.get("set_anchor") == [1], custom_panel  # Centered along the top edge.
            assert custom_panel.get("set_margin", []) == [14, 14, 14, 14], custom_panel
            panel_box = (360, 14, 1560, 62)
            time.sleep(0.4)
            restored_logo_image = capture("restored-lunadash-launcher-1920x1080", (1920, 1080))
            launcher_box = (940, 18, 980, 58)
            logo_change = ImageChops.difference(custom_logo_image.crop(launcher_box), restored_logo_image.crop(launcher_box)).convert("L")
            changed_logo_pixels = logo_change.point(lambda value: 255 if value > 15 else 0).histogram()[255]
            assert changed_logo_pixels > 150, "The saved custom image did not visibly replace the centered LunaDash logo"
            request("appearance", json.dumps({"themeMode": "light"}))
            wait_for(lambda state: state["palette"]["dark"] is False, "light panel reference palette")
            light_panel, _ = capture_palette_change("custom-panel-light-1920x1080", restored_logo_image, True)
            request("appearance", json.dumps({"themeMode": "dark"}))
            dark_state = wait_for(lambda state: state["palette"]["dark"] is True, "dark panel palette")
            dark_panel, darkened = capture_palette_change("custom-panel-dark-1920x1080", light_panel, False)
            snapshots.append({"scene": "custom-panel-dark", "panel": custom_panel,
                              "panelBox": panel_box, "darkenedPixels": darkened, "state": dark_state})
            for count in (1, 2):
                request("launch-default", "files")
                custom_windows = wait_for(lambda state: clients_settled(state, count),
                                          f"custom-panel work area with {count} clients")
                verify_tiles(custom_windows)
            capture("custom-panel-window-work-area-1920x1080", (1920, 1080))
            snapshots.append({"scene": "custom-panel-window-work-area", "state": custom_windows})
            for client in application_clients(custom_windows):
                request("close", client["id"])
            wait_for(lambda state: not application_clients(state), "custom-panel clients close")
            assert not any(layer["namespace"] == "lunadash-dock" for layer in layers_from_log(log_path)), (
                "The disabled bottom dock created a layer-shell surface"
            )
            reject_qml_errors(log_path)
            print("Quickshell layout passed: Settings UI saves, custom dark panel, bounded popup, no dock, fitted client buffers, and rounded glass/opaque window screenshots.")
        except BaseException:
            try:
                failure_state = request()
                snapshots.append({"scene": "failure", "state": failure_state})
                capture("failure", (failure_state["display"]["width"], failure_state["display"]["height"]))
            except Exception as capture_error:
                print(f"Could not capture the failed UI state: {capture_error}", file=sys.stderr)
            print(log_path.read_text(errors="replace")[-20000:], file=sys.stderr)
            raise
        finally:
            (evidence / "snapshots.json").write_text(json.dumps(snapshots, indent=2))
            (evidence / "ui-actions.json").write_text(json.dumps(ui_actions, indent=2))
            (evidence / "layer-surfaces.json").write_text(json.dumps(layers_from_log(log_path), indent=2))
            for terminal in terminals:
                if terminal.poll() is None:
                    terminal.terminate()
                try:
                    terminal.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    terminal.kill()
                    terminal.wait(timeout=3)
            if keyboard_keeper is not None and keyboard_keeper.poll() is None:
                keyboard_keeper.terminate()
                try:
                    keyboard_keeper.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    keyboard_keeper.kill()
                    keyboard_keeper.wait(timeout=3)
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
