"""Check presented pixels for switching, maximize/restore and client-initiated close."""
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time
from PIL import Image

build = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="lunadash-motion-") as directory:
    root = Path(directory)
    env = os.environ | {"XDG_RUNTIME_DIR": directory, "XDG_CONFIG_HOME": directory,
        "XDG_STATE_HOME": directory, "XDG_DATA_HOME": directory,
        "WLR_BACKENDS": "headless", "WLR_HEADLESS_OUTPUTS": "1", "WLR_RENDERER": "pixman",
        "LUDASH_DISABLE_XWAYLAND": "1", "LUDASH_SKIP_SETUP": "1",
        "LUNADASH_DISABLE_FCITX": "1", "QT_QPA_PLATFORMTHEME": "generic", "GTK_USE_PORTAL": "0"}
    for key in ("WAYLAND_DISPLAY", "DISPLAY", "LUNADASH_PUBLISH_ACTIVATION_ENV"):
        env.pop(key, None)
    control = root / "lunadash-motion-control"
    command_file = root / "action"
    fixture = root / "motion.py"
    fixture.write_text('''import gi
from pathlib import Path
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, GLib
GLib.set_prgname("org.lunadash.MotionFixture")
windows = []
for title, color in (("Motion red", (0.92, 0.20, 0.10)), ("Motion blue", (0.10, 0.30, 0.85))):
    window = Gtk.Window(title=title)
    window.set_default_size(650, 420)
    canvas = Gtk.DrawingArea()
    def draw(widget, context, rgb=color):
        context.set_source_rgb(*rgb)
        context.paint()
    canvas.connect("draw", draw)
    window.add(canvas)
    window.show_all()
    windows.append(window)
def action():
    path = Path(''' + repr(str(command_file)) + ''')
    if path.exists():
        command = path.read_text()
        path.unlink()
        if command == "maximize": windows[0].maximize()
        elif command == "restore": windows[0].unmaximize()
        elif command == "close": windows[0].destroy()
        elif command == "maximize-blue": windows[1].maximize()
    return True
GLib.timeout_add(10, action)
Gtk.main()
''')

    def request(method="status", value=""):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(5)
            connection.connect(str(control))
            connection.sendall(json.dumps({"method": method, "value": str(value)}).encode() + b"\n")
            data = b""
            while b"\n" not in data:
                chunk = connection.recv(65536)
                assert chunk and len(data) < 2 * 1024 * 1024
                data += chunk
            return json.loads(data)

    def until(predicate):
        deadline = time.monotonic() + 8
        while True:
            state = request()
            if predicate(state):
                return state
            assert time.monotonic() < deadline, state
            time.sleep(.01)

    def client(state, title="Motion red"):
        return next(c for c in state["clients"] if c["title"] == title)

    def capture(name):
        path = build / ("motion-" + name + ".png")
        assert "error" not in request("capture", str(path))
        image = Image.open(path).convert("RGB")
        # Isolate the red fixture from the blue window and neutral background.
        mask = Image.new("L", image.size)
        mask.putdata([255 if r > 170 and g < 110 and b < 110 else 0 for r, g, b in image.getdata()])
        return mask.getbbox()

    with (build / "window-animations.log").open("w") as log:
        process = subprocess.Popen([str(build / "lunadash-compositor"), "--no-shell", "--socket", "lunadash-motion"], env=env, stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 10
            while not control.exists():
                assert process.poll() is None and time.monotonic() < deadline
                time.sleep(.05)
            request("appearance", '{"animations":true,"animationDuration":600}')
            request("launch-command", json.dumps(["python3", str(fixture)]))
            state = until(lambda s: len([c for c in s["clients"] if c["mapped"]]) == 2 and not s["activeAnimations"])
            red = client(state)["id"]
            blue = client(state, "Motion blue")["id"]
            before_x = client(state)["renderX"]
            state = request("focus", red)
            assert state["activeAnimations"] and client(state)["renderX"] != client(state)["x"], state
            time.sleep(.13)
            middle = client(request())["renderX"]
            assert min(before_x, client(state)["x"]) < middle < max(before_x, client(state)["x"]), state
            assert capture("switch-middle"), "Switch never presents the selected window"
            until(lambda s: not s["activeAnimations"])
            initial = capture("initial")
            assert initial
            initial_width = initial[2] - initial[0]
            restore_width = client(request())["width"]

            command_file.write_text("maximize")
            until(lambda s: client(s)["maximized"] and s["activeAnimations"])
            time.sleep(.12)
            expanded_mid = capture("expand-middle")
            until(lambda s: not s["activeAnimations"])
            expanded = capture("expanded")
            assert initial_width < expanded_mid[2] - expanded_mid[0] < expanded[2] - expanded[0], (initial, expanded_mid, expanded)

            command_file.write_text("restore")
            until(lambda s: not client(s)["maximized"] and s["activeAnimations"])
            time.sleep(.12)
            restored_mid = capture("restore-middle")
            state = until(lambda s: not s["activeAnimations"])
            restored = capture("restored")
            assert restored[2] - restored[0] < restored_mid[2] - restored_mid[0] < expanded[2] - expanded[0], (restored, restored_mid, expanded)
            assert client(state)["width"] == restore_width, "Restore must recover the original column width"

            for index in range(12):
                request("focus", blue if index % 2 == 0 else red)
            until(lambda s: not s["activeAnimations"])
            command_file.write_text("close")
            until(lambda s: not any(c["id"] == red and c["mapped"] for c in s["clients"]))
            time.sleep(.23)
            closing = capture("close-middle")
            assert closing and closing[2] - closing[0] < initial_width, "Client-initiated close must keep a shrinking last frame"
            until(lambda s: not s["activeAnimations"])
            assert capture("closed") is None, "Closed window snapshot was not released"

            request("appearance", '{"animations":false}')
            command_file.write_text("maximize-blue")
            state = until(lambda s: client(s, "Motion blue")["maximized"])
            assert state["activeAnimations"] == 0
            print("Window animation pixels passed: sliding selection, intermediate expand/restore sizes, original width restore, rapid retarget, client-side close shrink/fade and reduced motion.")
        except BaseException:
            log.flush()
            print((build / "window-animations.log").read_text(), file=sys.stderr)
            raise
        finally:
            try:
                request("quit")
            except (OSError, ValueError):
                pass
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)
