"""Load every settings page and validate settings without mutating host services."""

import json
import os
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

build = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="ludash-settings-") as runtime:
    env = os.environ | {
        "XDG_RUNTIME_DIR": runtime,
        "XDG_CONFIG_HOME": runtime,
        "QT_QPA_PLATFORM": "xcb",
        "QT_XCB_GL_INTEGRATION": "xcb_egl",
        "LIBGL_ALWAYS_SOFTWARE": "1",
        "LUDASH_TEST_SETTINGS": "1",
        "LUDASH_SKIP_SETUP": "1",
        "LUDASH_LANGUAGE": "en_US",
        "QT_FORCE_STDERR_LOGGING": "1",
    }
    for key in ("MESA_GL_VERSION_OVERRIDE", "MESA_GLSL_VERSION_OVERRIDE"):
        env.pop(key, None)
    control = runtime + "/ludash-settings-control"

    def request(method="status", value=""):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(3)
            connection.connect(control)
            connection.sendall(
                json.dumps({"method": method, "value": str(value)}).encode() + b"\n"
            )
            result = b""
            while b"\n" not in result:
                chunk = connection.recv(65536)
                if not chunk or len(result) > 1024 * 1024:
                    raise RuntimeError("Invalid IPC response")
                result += chunk
            return json.loads(result)

    with open(build / "settings.log", "w+") as log:
        process = subprocess.Popen(
            [
                str(build / "ludash-compositor"),
                "--socket",
                "ludash-settings",
                "--exit-after",
                "27500",
                "--screenshot",
                str(build / "settings-preview.png"),
            ],
            env=env,
            stdout=log,
            stderr=log,
        )
        try:
            deadline = time.monotonic() + 7
            while True:
                try:
                    if request()["layerSurfaces"] >= 2:
                        break
                except OSError:
                    pass
                assert time.monotonic() < deadline, "Shell did not map"
                time.sleep(0.1)
            pages = (
                "general",
                "modules",
                "windows",
                "shortcuts",
                "display",
                "input",
                "sound",
                "network",
                "bluetooth",
                "power",
                "applications",
                "privacy",
                "system",
                "devices",
                "about",
                "appearance",
            )
            for page in pages:
                result = request("open-settings", page)
                assert "error" not in result, result
                time.sleep(0.85)
            assert request()["layerSurfaces"] >= 3, "Settings did not map"
            assert "error" not in request(
                "appearance",
                json.dumps(
                    {
                        "workspaceCount": 6,
                        "masterRatio": 60,
                        "keyboardLayout": "gb",
                        "keyRepeatRate": 30,
                        "keyRepeatDelay": 400,
                        "fontFamily": "monospace",
                    }
                ),
            )
            state = request()
            assert state["input"] == {
                "layout": "gb",
                "repeatRate": 30,
                "repeatDelay": 400,
            }, state["input"]
            assert state["shortcuts"]["focusLeft"] == "Meta+H", state["shortcuts"]
            assert "error" not in request("shortcuts", '{"focusLeft":"Meta+U"}')
            assert request()["shortcuts"]["focusLeft"] == "Meta+U"
            assert "error" in request("shortcuts", '{"focusLeft":"Meta+L"}')
            assert "error" in request("shortcuts", '{"focusLeft":"Alt+U"}')
            assert "error" not in request("reset-shortcuts")
            assert request()["shortcuts"]["focusLeft"] == "Meta+H"
            assert "error" not in request("workspace", 5)
            assert request()["workspace"] == 5
            request("appearance", '{"workspaceCount":2}')
            assert request()["workspace"] == 1, (
                "Removed workspace stranded the active desktop"
            )
            assert "error" in request("audio", '{"device":"output","volume":101}')
            assert "error" in request("power-profile", "arbitrary-profile")
            assert "error" in request("system-tool", "/bin/sh")
            assert "error" in request("open-settings", "../../shell")
            assert "error" in request("desktop-size", "0x0")
            request("reset-preferences")
            assert request()["appearance"]["workspaceCount"] == 4
            assert process.wait(timeout=34) == 0, (
                "Settings session did not close cleanly"
            )
            log.flush()
            log.seek(0)
            output = log.read()
            for page in pages:
                assert "Settings page loaded: " + page in output, (
                    "Settings page was not loaded: " + page
                )
            for failure in (
                "ReferenceError",
                "TypeError",
                "Error loading QML",
                "Cannot assign to non-existent",
                "is not a type",
            ):
                assert failure not in output, output
            print(
                "Settings passed: 16 pages, shortcuts, keyboard application, workspace bounds, reset and invalid service-command rejection."
            )
        except BaseException:
            log.flush()
            log.seek(0)
            print(log.read(), file=sys.stderr)
            raise
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
