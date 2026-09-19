"""Verify application identity and task selection on an isolated headless output."""
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time

build = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="lunadash-tasks-") as directory:
    root = Path(directory)
    env = os.environ | {"XDG_RUNTIME_DIR": directory, "XDG_CONFIG_HOME": directory,
        "XDG_STATE_HOME": directory, "XDG_DATA_HOME": directory,
        "WLR_BACKENDS": "headless", "WLR_HEADLESS_OUTPUTS": "1", "WLR_RENDERER": "pixman",
        "LUDASH_DISABLE_XWAYLAND": "1", "LUDASH_SKIP_SETUP": "1",
        "LUNADASH_DISABLE_FCITX": "1", "QT_QPA_PLATFORMTHEME": "generic", "GTK_USE_PORTAL": "0"}
    for key in ("WAYLAND_DISPLAY", "DISPLAY", "LUNADASH_PUBLISH_ACTIVATION_ENV"):
        env.pop(key, None)
    control = root / "lunadash-tasks-control"
    fixture = root / "windows.py"
    fixture.write_text('''import gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, GLib
GLib.set_prgname("org.lunadash.TaskFixture")
windows = []
for title in ("File browser documentation", "Terminal settings", "Console monitor", "Fourth window"):
    window = Gtk.Window(title=title)
    window.add(Gtk.Label(label=title))
    window.set_default_size(700, 400)
    window.show_all()
    windows.append(window)
Gtk.main()
''')

    def request(method="status", value=""):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(3)
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
            time.sleep(.04)

    def selected(state, window):
        client = next(c for c in state["clients"] if c["id"] == window)
        assert client["mapped"] and client["focused"] and not client["minimized"], client
        assert 0 <= client["x"] and client["x"] + client["width"] <= state["display"]["width"], client
        assert sum(c["focused"] for c in state["clients"]) == 1, state
        groups = state["tiling"]["groups"]
        group = next(g for g in groups if any(m["window"] == window for m in g["members"]))
        assert group["focused"], group
        assert next(m for m in group["members"] if m["window"] == window)["focused"], group

    with (build / "window-tasks.log").open("w") as log:
        process = subprocess.Popen([str(build / "lunadash-compositor"), "--no-shell", "--socket", "lunadash-tasks"], env=env, stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 10
            while not control.exists():
                assert process.poll() is None and time.monotonic() < deadline
                time.sleep(.05)
            # Missing compatibility support must return a useful error before
            # attempting to start a native helper with an unusable X display.
            assert "XWayland" in request("launch-command", '["discord"]')["error"]
            assert "error" not in request("appearance", '{"animations":false}')
            assert "error" not in request("launch-command", json.dumps(["python3", str(fixture)]))
            state = until(lambda s: len([c for c in s["clients"] if c["mapped"]]) == 4)
            windows = sorted(c["id"] for c in state["clients"])
            for client in state["clients"]:
                assert client["appId"] and client["icon"] == client["appId"], client
            for window in (windows[-1], windows[0], windows[2], windows[1]):
                selected(request("focus", window), window)
            request("group-window", json.dumps({"window": windows[1], "target": windows[0]}))
            selected(request("focus", windows[1]), windows[1])
            state = request("minimize", windows[1])
            member = next(m for g in state["tiling"]["groups"] for m in g["members"] if m["window"] == windows[1])
            assert member["minimized"] and not member["focused"], member
            selected(request("focus", windows[1]), windows[1])
            request("workspace", 1)
            state = request("focus", windows[0])
            assert state["workspace"] == 0
            selected(state, windows[0])
            request("close", windows[2])
            until(lambda s: not any(c["id"] == windows[2] for c in s["clients"]))
            assert "error" in request("focus", windows[2]), "Stale task must not select a different client"
            selected(request("focus", windows[-1]), windows[-1])
            print("Window tasks passed: title-independent icons, offscreen focus, grouped selection, restore, workspace switch and stale IDs.")
        except BaseException:
            log.flush()
            print((build / "window-tasks.log").read_text(), file=sys.stderr)
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
