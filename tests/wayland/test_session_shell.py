#!/usr/bin/env python3
"""Run the LunaDash compositor with its Slint shell and check that both talk to each other.

Usage: test_session_shell.py BUILD_DIR

The build must include SlintLu (configure with -DLUDASH_SLINTLU_SOURCE_DIR); the compositor
then finds it at BUILD_DIR/slintlu/release/slintlu. Run inside `dbus-run-session`.
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
compositor = build / "lunadash-compositor"
slintlu = build / "slintlu/release/slintlu"
evidence = build / "ci-evidence/shell"
evidence.mkdir(parents=True, exist_ok=True)
assert slintlu.is_file(), f"SlintLu was not built: {slintlu}"


def wait_for(description, predicate, timeout=20):
    deadline = time.monotonic() + timeout
    last = None
    while time.monotonic() < deadline:
        try:
            last = predicate()
            if last:
                return last
        except (OSError, ValueError, subprocess.SubprocessError):
            pass
        time.sleep(0.2)
    raise AssertionError(f"Timed out waiting for {description} (last: {last!r})")


with tempfile.TemporaryDirectory(prefix="lunadash-shell-") as runtime:
    socket_name = "lunadash-shell-test"
    env = os.environ | {
        "XDG_RUNTIME_DIR": runtime,
        "XDG_CONFIG_HOME": runtime + "/config",
        "XDG_STATE_HOME": runtime + "/state",
        "XDG_CACHE_HOME": runtime + "/cache",
        "XDG_DATA_HOME": runtime + "/data",
        "WLR_BACKENDS": "headless",
        "WLR_RENDERER": "pixman",
        "WLR_HEADLESS_OUTPUTS": "1",
        "LUDASH_DISABLE_XWAYLAND": "1",
        "LUDASH_SKIP_SETUP": "1",
        "LUDASH_SHELL_RENDERER": "software",
        # Never check a real password: this PAM service does not exist.
        "SLINTLU_PAM_SERVICE": "lunadash-test-nonexistent-service",
        "RUST_LOG": "info",
    }
    for name in ("WAYLAND_DISPLAY", "DISPLAY", "NIRI_SOCKET", "SWAYSOCK", "I3SOCK",
                 "HYPRLAND_INSTANCE_SIGNATURE", "LUNADASH_CONTROL",
                 # Without it the shell never registers as the login session's polkit agent.
                 "XDG_SESSION_ID"):
        env.pop(name, None)
    control = f"{runtime}/{socket_name}-control"
    client_env = env | {"WAYLAND_DISPLAY": socket_name}

    def request(method="status", value=""):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(2)
            connection.connect(control)
            connection.sendall(json.dumps({"method": method, "value": value}).encode() + b"\n")
            output = b""
            while b"\n" not in output:
                chunk = connection.recv(1 << 20)
                if not chunk:
                    raise OSError("control connection closed")
                output += chunk
            return json.loads(output)

    def shell(*args):
        return subprocess.run([str(slintlu), "msg", *args], env=client_env, capture_output=True,
                              text=True, timeout=5, check=True).stdout.strip()

    with open(evidence / "session.log", "w+") as log:
        process = subprocess.Popen(
            [str(compositor), "--socket", socket_name, "--exit-after", "40000",
             "--state", str(evidence / "state.json")],
            env=env, stdout=log, stderr=log)
        try:
            # The compositor starts the shell; SlintLu's IPC answers once it is loaded.
            wait_for("the shell's IPC socket", lambda: "version" in shell("ping"), timeout=30)
            assert shell("get", "Compositor.name") == "lunadash", "SlintLu did not detect LunaDash"
            assert shell("get", "Compositor.has-commands") == "true"
            # Wallpaper, panel and dock are layer surfaces.
            base = wait_for("the shell's panels",
                            lambda: (n := request()["layerSurfaces"]) >= 3 and n)
            status = request()
            assert shell("get", "Compositor.language") == status["language"], status["language"]

            # Compositor → shell: LunaDash asks for Settings (a shortcut does the same).
            assert "error" not in request("open-settings", "appearance")
            wait_for("the settings overlay", lambda: request()["layerSurfaces"] > base)
            subprocess.run(["grim", str(evidence / "settings.png")], env=client_env, timeout=10)
            shell("call", "Shared.set-string", "overlay", "")
            wait_for("the overlay to close", lambda: request()["layerSurfaces"] == base)

            # Shell → compositor: opening the launcher is reported, so Meta toggles it.
            shell("call", "Shared.set-string", "overlay", "launcher")
            wait_for("launcherOpen", lambda: request()["launcherOpen"] is True)
            shell("call", "Shared.set-string", "overlay", "")
            wait_for("launcher closed", lambda: request()["launcherOpen"] is False)

            subprocess.run(["grim", str(evidence / "desktop.png")], env=client_env, timeout=10)
            assert not request().get("processFailure"), "the compositor reported a failure"
            assert process.poll() is None, "the compositor exited early"
        except BaseException:
            log.flush()
            log.seek(0)
            print(log.read()[-6000:], file=sys.stderr)
            raise
        finally:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()

print(f"Slint shell session passed: {base} layer surfaces, settings and launcher round trips")
