"""Exercise asynchronous region capture and display rollback on an isolated headless output.

Brightness and selection helpers are fixtures; grim captures the real Wayland output.
No physical monitor, host backlight or host session is changed.
"""
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile
import time

build = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="lunadash-controls-") as directory:
    root = Path(directory)
    tools = root / "bin"
    tools.mkdir()
    selector_mode = root / "selection"
    selector_mode.write_text("select")
    selector = tools / "slurp"
    selector.write_text('#!/bin/sh\nsleep 0.3\ncase "$(cat "$LUNADASH_SELECTION_FIXTURE")" in\n cancel) exit 1;;\n invalid) printf "bad geometry\\n";;\n *) printf "10,20 160x90\\n";;\nesac\n')
    selector.chmod(0o700)
    brightness = tools / "brightnessctl"
    brightness.write_text('#!/bin/sh\nprintf "intel_backlight,backlight,12000,60%%,20000\\n"\n')
    brightness.chmod(0o700)
    (root / "Pictures").mkdir()
    (root / "user-dirs.dirs").write_text('XDG_PICTURES_DIR="' + str(root / "Pictures") + '"\n')
    env = os.environ | {"XDG_RUNTIME_DIR": directory,
        "XDG_CONFIG_HOME": directory, "XDG_DATA_HOME": directory,
        "PATH": str(tools) + os.pathsep + os.environ["PATH"],
        "LUNADASH_SELECTION_FIXTURE": str(selector_mode),
        "WLR_BACKENDS": "headless", "WLR_HEADLESS_OUTPUTS": "1", "WLR_RENDERER": "pixman",
        "LUDASH_DISABLE_XWAYLAND": "1", "LUDASH_SKIP_SETUP": "1", "LUNADASH_DISABLE_FCITX": "1"}
    for key in ("WAYLAND_DISPLAY", "DISPLAY", "LUNADASH_PUBLISH_ACTIVATION_ENV"):
        env.pop(key, None)
    control = root / "lunadash-controls-control"
    def request(method="status", value=""):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(3)
            connection.connect(str(control))
            connection.sendall(json.dumps({"method":method, "value":value}).encode() + b"\n")
            data = b""
            while b"\n" not in data:
                chunk = connection.recv(65536)
                assert chunk and len(data) < 2 * 1024 * 1024
                data += chunk
            return json.loads(data)
    def until(predicate, seconds=8):
        deadline = time.monotonic() + seconds
        while True:
            value = request()
            if predicate(value): return value
            assert time.monotonic() < deadline, value
            time.sleep(.03)
    with (build / "desktop-controls.log").open("w") as log:
        process = subprocess.Popen([str(build / "lunadash-compositor"), "--no-shell", "--socket", "lunadash-controls"], env=env, stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 10
            while not control.exists():
                assert process.poll() is None and time.monotonic() < deadline
                time.sleep(.05)
            state = until(lambda s: s["brightness"].get("available"))
            assert state["brightness"]["percent"] == 60, state["brightness"]
            assert state["shortcuts"]["screenshot"] == "Meta+Shift+S"
            for value in ("-1", "101", "not-a-number"):
                assert "error" in request("brightness", value)
            started = time.monotonic()
            result = request("screenshot")
            assert result["pending"] and time.monotonic() - started < .25, result
            assert request()["screenCapture"]["busy"]
            state = until(lambda s: s["screenCapture"]["phase"] == "saved")
            path = Path(state["screenCapture"]["lastCapture"])
            png = path.read_bytes()
            assert png[:8] == b"\x89PNG\r\n\x1a\n"
            assert struct.unpack(">II", png[16:24]) == (160, 90), "Capture must contain only the selected rectangle"
            assert path.stat().st_mode & 0o077 == 0
            selector_mode.write_text("cancel")
            request("screenshot")
            state = until(lambda s: s["screenCapture"]["phase"] == "cancelled")
            assert state["screenCapture"]["lastCapture"] == str(path)
            assert not state["screenCapture"]["error"]
            selector_mode.write_text("invalid")
            request("screenshot")
            state = until(lambda s: s["screenCapture"]["phase"] == "failed")
            assert state["screenCapture"]["error"]
            assert len(list(path.parent.glob("*.png"))) == 1
            initial = request()["display"]
            assert "error" in request("display-configure", json.dumps({"scale":0}))
            assert "error" in request("display-configure", json.dumps({"mode":"99999x99999@1000"}))
            state = request("display-configure", json.dumps({"scale":1.5}))
            assert state["display"]["pending"] and state["display"]["scale"] == 1.5, state
            request("display-revert")
            assert request()["display"]["scale"] == initial["scale"]
            request("display-configure", json.dumps({"scale":1.25}))
            request("display-confirm")
            assert not request()["display"]["pending"]
            request("display-configure", json.dumps({"scale":1.5}))
            state = until(lambda s: not s["display"]["pending"], 18)
            assert state["display"]["scale"] == 1.25, state["display"]
            print("Region dimensions, selection cancellation, responsive IPC, backlight parsing and display confirmation/rollback passed.")
        finally:
            process.terminate()
            try: process.wait(timeout=4)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=4)
