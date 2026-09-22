"""Stress GLX notification teardown; optionally use the real gsr-notify binary."""
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time

build = Path(sys.argv[1]).resolve()
helper = os.environ.get("LUNADASH_TEST_GSR_NOTIFY")
command = ([helper, "--text", "Saved recording", "--timeout", "0.3"] if helper else
           [str(build / "lunadash-x11-notification-test")])
with tempfile.TemporaryDirectory(prefix="lunadash-notification-") as runtime:
    env = os.environ | {
        "XDG_RUNTIME_DIR": runtime, "XDG_CONFIG_HOME": runtime,
        "XDG_DATA_HOME": runtime, "WLR_BACKENDS": "headless",
        "WLR_HEADLESS_OUTPUTS": "1", "WLR_RENDERER": "pixman",
        "LUDASH_SKIP_SETUP": "1", "LUNADASH_DISABLE_FCITX": "1",
    }
    for key in ("LUDASH_DISABLE_XWAYLAND", "XAUTHORITY", "WAYLAND_DISPLAY"):
        env.pop(key, None)

    def request(method="status", value=""):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(3)
            connection.connect(runtime + "/notification-test-control")
            connection.sendall(json.dumps({"method": method, "value": value}).encode() + b"\n")
            data = b""
            while b"\n" not in data:
                chunk = connection.recv(65536)
                assert chunk, "Compositor closed IPC during notification teardown"
                data += chunk
            return json.loads(data)

    with (build / "xwayland-notifications.log").open("w+") as log:
        process = subprocess.Popen([
            str(build / "lunadash-compositor"), "--no-shell", "--socket",
            "notification-test", "--exit-after", "60000"], env=env, stdout=log, stderr=log)
        child = None
        try:
            deadline = time.monotonic() + 8
            while True:
                assert process.poll() is None, "Compositor failed to start"
                try:
                    state = request()
                    break
                except OSError:
                    assert time.monotonic() < deadline, "IPC did not start"
                    time.sleep(0.05)
            env["DISPLAY"] = state["xwayland"]["display"]
            mapped = 0
            for iteration in range(25):
                child = subprocess.Popen(command, env=env, stdout=log, stderr=log)
                deadline = time.monotonic() + 5
                saw_map = False
                while child.poll() is None:
                    assert process.poll() is None, "Notification crashed compositor"
                    assert time.monotonic() < deadline, "Notification hung"
                    saw_map |= any(c.get("x11") and c["mapped"] for c in request()["clients"])
                    time.sleep(0.025)
                assert child.returncode == 0, "Notification helper failed"
                mapped += saw_map
                deadline = time.monotonic() + 3
                while any(c.get("x11") for c in request()["clients"]):
                    assert time.monotonic() < deadline, "Notification client leaked"
                    time.sleep(0.025)
                assert process.poll() is None, "Notification teardown crashed compositor"
            assert mapped >= 10, f"Only {mapped} notification windows actually mapped"
            request("quit", "confirm")
            assert process.wait(timeout=8) == 0
            print(f"Notification lifecycle passed: 25 teardowns, {mapped} observed maps, clean exit")
        except BaseException:
            log.flush()
            log.seek(0)
            print(log.read()[-18000:], file=sys.stderr)
            raise
        finally:
            for owned in (child, process):
                if owned is not None and owned.poll() is None:
                    owned.terminate()
                    try:
                        owned.wait(timeout=4)
                    except subprocess.TimeoutExpired:
                        owned.kill()
                        owned.wait(timeout=4)
