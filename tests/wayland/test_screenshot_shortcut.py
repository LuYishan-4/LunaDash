"""Real Meta+Shift+S, slurp selection and grim capture on an isolated Xvfb host."""
import json
import os
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import time

build = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="lunadash-screenshot-") as directory:
    root = Path(directory)
    (root / "user-dirs.dirs").write_text(f'XDG_PICTURES_DIR="{root}/Pictures"\n')
    env = os.environ | {
        "XDG_RUNTIME_DIR": directory, "XDG_CONFIG_HOME": directory,
        "XDG_STATE_HOME": directory, "XDG_DATA_HOME": directory,
        "WLR_BACKENDS": "x11", "WLR_X11_OUTPUTS": "1", "WLR_RENDERER": "pixman",
        "LIBGL_ALWAYS_SOFTWARE": "1", "LUDASH_DISABLE_XWAYLAND": "1",
        "LUDASH_SKIP_SETUP": "1", "LUNADASH_DISABLE_FCITX": "1",
        "QT_QPA_PLATFORMTHEME": "generic",
    }
    for key in ("WAYLAND_DISPLAY", "LUNADASH_PUBLISH_ACTIVATION_ENV"):
        env.pop(key, None)
    control = root / "lunadash-screenshot-test-control"
    log_path = build / "screenshot-shortcut.log"

    def request(method="status", value=""):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(3)
            connection.connect(str(control))
            connection.sendall(json.dumps({"method": method, "value": value}).encode() + b"\n")
            data = b""
            while b"\n" not in data:
                chunk = connection.recv(65536)
                assert chunk and len(data) < 2 * 1024 * 1024
                data += chunk
            return json.loads(data)

    def wait_for(predicate, message):
        deadline = time.monotonic() + 8
        while True:
            assert process.poll() is None, log_path.read_text()
            try:
                value = predicate()
                if value:
                    return value
            except (OSError, ValueError):
                pass
            assert time.monotonic() < deadline, message + "\n" + log_path.read_text()
            time.sleep(.03)

    def input_events(*arguments):
        subprocess.run(["xdotool", *arguments], env=env, check=True, timeout=4)

    def selection_visible():
        state = request()
        return state["screenCapture"]["phase"] == "selecting" and state["layerSurfaces"] == 1

    def capture_finished(phase):
        state = request()
        capture = state["screenCapture"]
        return state if capture["phase"] == phase and not capture["busy"] and state["layerSurfaces"] == 0 else None

    with log_path.open("w") as log:
        process = subprocess.Popen(
            [str(build / "lunadash-compositor"), "--no-shell", "--socket", "lunadash-screenshot-test"],
            env=env, stdout=log, stderr=log)
        try:
            wait_for(control.exists, "No control socket")
            assert request()["shortcuts"]["screenshot"] == "Meta+Shift+S"
            host = subprocess.check_output(
                ["xdotool", "search", "--onlyvisible", "--name", ".*"], env=env, text=True).splitlines()[-1]
            input_events("windowfocus", "--sync", host, "mousemove", "--window", host, "100", "100")
            input_events("key", "super+shift+s")
            wait_for(selection_visible, "Screenshot shortcut did not show the real slurp overlay")
            input_events("mousemove", "--window", host, "120", "160", "mousedown", "1")
            time.sleep(.1)
            input_events("mousemove", "--window", host, "359", "299")
            time.sleep(.1)
            input_events("mouseup", "1")
            state = wait_for(lambda: capture_finished("saved"), "Selected region was not saved")
            path = Path(state["screenCapture"]["lastCapture"])
            png = path.read_bytes()
            assert png[:8] == b"\x89PNG\r\n\x1a\n"
            # slurp releases differ on whether the endpoint pixel is included.
            dimensions = struct.unpack(">II", png[16:24])
            assert dimensions in ((239, 139), (240, 140)), dimensions
            assert path.parent == root / "Pictures" / "Screenshots", path
            assert path.stat().st_mode & 0o077 == 0
            shutil.copyfile(path, build / "shortcut-region.png")

            # Repeating the shortcut must not steal Escape from the selector.
            input_events("key", "super+shift+s")
            wait_for(selection_visible, "Selector did not reopen after capture")
            input_events("key", "super+shift+s", "Escape")
            cancelled = wait_for(lambda: capture_finished("cancelled"), "Escape did not cancel selection")
            assert not cancelled["screenCapture"]["error"], cancelled["screenCapture"]
            assert cancelled["screenCapture"]["lastCapture"] == str(path)
            assert len(list(path.parent.glob("*.png"))) == 1

            input_events("key", "super+shift+s")
            wait_for(selection_visible, "Selector did not reopen after cancellation")
            input_events("key", "Escape")
            wait_for(lambda: capture_finished("cancelled"), "Second cancellation failed")
            print("Screenshot shortcut passed: real Meta+Shift+S, slurp overlay, dragged region dimensions, private PNG, repeated shortcut, Escape and reopen.")
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
