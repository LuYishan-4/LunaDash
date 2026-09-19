"""Run the complete shell on an isolated headless compositor (Arch CI)."""
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import tempfile
import time

build = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="lunadash-shell-") as directory:
    root = Path(directory)
    fixture_bin = root / "bin"
    fixture_bin.mkdir()
    ddc = fixture_bin / "ddcutil"
    ddc.write_text('#!/bin/sh\nif [ "$1" = detect ]; then\n printf "Display 1\\n I2C bus: /dev/i2c-7\\n Monitor: DEL:Fixture monitor:123\\n"\nelse\n printf "VCP 10 C 80 200\\n"\nfi\n')
    ddc.chmod(0o700)
    env = os.environ | {"XDG_RUNTIME_DIR": directory, "XDG_CONFIG_HOME": directory,
        "XDG_DATA_HOME": directory, "XDG_STATE_HOME": directory, "XDG_CACHE_HOME": directory,
        "WLR_BACKENDS": "headless", "WLR_HEADLESS_OUTPUTS": "1", "WLR_RENDERER": "pixman",
        "QT_QPA_PLATFORM": "wayland", "QT_QUICK_BACKEND": "software",
        "LUDASH_DISABLE_XWAYLAND": "1", "LUDASH_SKIP_SETUP": "1", "LUNADASH_DISABLE_FCITX": "1",
        "LUDASH_TEST_SETTINGS": "1", "PATH": str(fixture_bin) + os.pathsep + os.environ["PATH"]}
    for key in ("WAYLAND_DISPLAY", "DISPLAY", "DBUS_SESSION_BUS_ADDRESS", "LUNADASH_PUBLISH_ACTIVATION_ENV"):
        env.pop(key, None)
    control = root / "lunadash-shell-test-control"
    def request(method="status", value=""):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(3)
            connection.connect(str(control))
            connection.sendall(json.dumps({"method":method,"value":value}).encode()+b"\n")
            data = b""
            while b"\n" not in data:
                chunk = connection.recv(65536)
                assert chunk and len(data)<2*1024*1024
                data += chunk
            return json.loads(data)
    log_path = build / "shell-startup.log"
    with log_path.open("w") as log:
        process = subprocess.Popen(["dbus-run-session", "--", str(build / "lunadash-compositor"), "--socket", "lunadash-shell-test"], env=env, stdout=log, stderr=log, start_new_session=True)
        try:
            deadline = time.monotonic()+20
            saw_splash = False
            while True:
                assert process.poll() is None, log_path.read_text()
                state = request() if control.exists() else {}
                layers = state.get("layerNamespaces", [])
                saw_splash |= "lunadash-startup" in layers
                if saw_splash and "lunadash-wallpaper" in layers and "lunadash-panel" in layers and "lunadash-startup" not in layers:
                    break
                assert time.monotonic()<deadline, log_path.read_text()
                time.sleep(.04)
            for language in ("en_US", "zh_TW", "zh_CN", "ja_JP"):
                previous_loads = log_path.read_text().count("Settings page loaded: display")
                assert "error" not in request("language", language)
                assert "error" not in request("open-settings", "display")
                deadline = time.monotonic()+8
                while ("lunadash-settings" not in request()["layerNamespaces"] or
                       log_path.read_text().count("Settings page loaded: display") <= previous_loads):
                    assert time.monotonic()<deadline, log_path.read_text()
                    time.sleep(.1)
            text = log_path.read_text()
            for error in ("ReferenceError:", "TypeError:", "Cannot assign", "Binding loop detected", "Failed to load configuration", "Settings page failed to load"):
                assert error not in text, text
            print("Full shell startup passed: animated splash maps and closes, wallpaper/panel map, localized Display settings open.")
        finally:
            try: request("quit")
            except (OSError, ValueError): pass
            try: process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGTERM)
                try: process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait(timeout=3)
