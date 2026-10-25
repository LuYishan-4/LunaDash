"""A session.json with a custom shell: no LunaDash shell starts, the custom shell and the
autostart commands do. Then "none", and an invalid file that falls back to LunaDash.

Usage: python3 tests/wayland/test_session_config.py BUILD_DIR (run inside dbus-run-session).
The launcher shortcut is not exercised: compositor shortcuts ignore virtual keyboards.
"""

import json
import os
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

build = Path(sys.argv[1]).resolve()
binary = build / "lunadash-compositor"
if not binary.exists():
    binary = build / "ludash-compositor"


def wait_for(description, predicate, timeout=15):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            if predicate():
                return
        except (OSError, ValueError, KeyError):
            pass
        time.sleep(0.1)
    raise AssertionError(f"timed out waiting for {description}")


def run_session(runtime, config, check):
    home = Path(runtime) / "home"
    (home / ".config/LunaDash").mkdir(parents=True, exist_ok=True)
    (home / ".config/LunaDash/session.json").write_text(json.dumps(config))
    env = os.environ | {
        "HOME": str(home),
        "XDG_RUNTIME_DIR": runtime,
        "XDG_CONFIG_HOME": str(home / ".config"),
        "XDG_CONFIG_DIRS": str(Path(runtime) / "no-system-config"),
        "XDG_STATE_HOME": str(home / ".local/state"),
        "XDG_CACHE_HOME": str(home / ".cache"),
        "WLR_BACKENDS": "headless",
        "WLR_RENDERER": "pixman",
        "WLR_HEADLESS_OUTPUTS": "1",
        "LUDASH_DISABLE_XWAYLAND": "1",
        "LUDASH_SKIP_SETUP": "1",
        "LANG": "C.UTF-8",
    }
    for name in ("WAYLAND_DISPLAY", "DISPLAY", "LUNADASH_SHELL"):
        env.pop(name, None)
    control = Path(runtime) / "session-config-control"

    def request(method="status", value=""):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(2)
            connection.connect(str(control))
            connection.sendall(json.dumps({"method": method, "value": value}).encode() + b"\n")
            output = b""
            while b"\n" not in output:
                chunk = connection.recv(65536)
                if not chunk:
                    raise ValueError("closed")
                output += chunk
            return json.loads(output)

    log = open(Path(runtime) / "compositor.log", "w+")
    process = subprocess.Popen([str(binary), "--socket", "session-config", "--exit-after", "20000"],
                               env=env, stdout=log, stderr=log)
    try:
        wait_for("the control socket", lambda: request()["session"] is not None)
        check(request, env | {"WAYLAND_DISPLAY": "session-config"})
    except BaseException:
        log.flush()
        log.seek(0)
        print(log.read()[-4000:], file=sys.stderr)
        raise
    finally:
        process.terminate()
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        log.close()


with tempfile.TemporaryDirectory(prefix="ludash-session-config-") as runtime:
    marks = Path(runtime) / "marks"
    marks.mkdir()

    def mark(name):
        # Writes a file and stays alive, like a bar or daemon would.
        return ["sh", "-c", f'touch "{marks}/{name}"; exec sleep 60']

    def custom(request, client_env):
        session = request()["session"]
        assert session["shell"] == "custom" and not session["error"], session
        wait_for("the custom shell", lambda: (marks / "shell").exists())
        wait_for("both autostart commands",
                 lambda: (marks / "first").exists() and (marks / "second").exists())
        time.sleep(1.5)
        state = request()
        assert state["layerSurfaces"] == 0, "the LunaDash shell started despite session.json"
        assert state["session"]["launcherCommand"][0] == "sh", state["session"]

    run_session(runtime, {
        "shell": "custom",
        "shellCommand": mark("shell"),
        "launcherCommand": ["sh", "-c", f'touch "{marks}/launcher"'],
        "autostart": [mark("first"), mark("second")],
    }, custom)

    def none(request, client_env):
        session = request()["session"]
        assert session["shell"] == "none" and not session["error"], session
        wait_for("autostart", lambda: (marks / "none-autostart").exists())
        time.sleep(1.5)
        assert request()["layerSurfaces"] == 0, "a shell started with \"shell\": \"none\""

    run_session(runtime, {"shell": "none", "autostart": [mark("none-autostart")]}, none)

    def invalid(request, client_env):
        session = request()["session"]
        assert session["shell"] == "lunadash" and session["error"], session

    run_session(runtime, {"shell": "custom"}, invalid)

print("Session config passed: custom shell, autostart, no shell, invalid file")
