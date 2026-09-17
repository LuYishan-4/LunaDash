"""Click the top panel with real pointer input and drive shell actions over IPC.

Coordinate sources, for the 1440x900 test screen with the default 40 px panel,
four workspaces and a zero panel margin:

* the workspace strip starts 8 px from the left edge with 3 px spacing, the
  selected 32 px wide segment is followed by 24 px wide segments, so the second
  workspace is centred on x = 55 (qml/panel/TopPanel.qml, qml/components/Segment.qml
  and the "panel" margin default in src/compositor/ShellModuleSchema/ShellModuleSchema.cpp);
* the centred selector is 28 + 2 + 54 + 2 + 28 = 114 px wide, which centres the
  overview button on 677, the launcher on 720 and settings on 763;
* the panel occupies y 0..40 with a zero margin, so the row centre is y = 20.
"""

import json
import os
import pathlib
import socket
import subprocess
import sys
import tempfile
import time

binary = pathlib.Path(sys.argv[1]).resolve() / "ludash-compositor"
WORKSPACE_2 = (55, 20)
LAUNCHER = (720, 20)
SETTINGS = (763, 20)

with tempfile.TemporaryDirectory(prefix="ludash-shell-test-") as runtime:
    env = os.environ | {
        "XDG_RUNTIME_DIR": runtime,
        "XDG_CONFIG_HOME": runtime,
        "QT_QPA_PLATFORM": "xcb",
        "QT_XCB_GL_INTEGRATION": "xcb_egl",
        "LUDASH_SKIP_SETUP": "1",
        "LIBGL_ALWAYS_SOFTWARE": "1",
        "QT_FORCE_STDERR_LOGGING": "1",
        "LANG": "C.UTF-8",
        "LC_ALL": "C.UTF-8",
    }
    for name in (
        "MESA_GL_VERSION_OVERRIDE",
        "MESA_GLSL_VERSION_OVERRIDE",
        "LUDASH_LANGUAGE",
    ):
        env.pop(name, None)
    control = runtime + "/ludash-shell-test-control"

    def request(method="status", value=""):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(5)
            connection.connect(control)
            connection.sendall(
                json.dumps({"method": method, "value": str(value)}).encode() + b"\n"
            )
            output = b""
            while b"\n" not in output:
                chunk = connection.recv(65536)
                if not chunk:
                    raise RuntimeError("Control connection closed without a response")
                output += chunk
                if len(output) > 1024 * 1024:
                    raise RuntimeError("Control response exceeded its limit")
            return json.loads(output)

    def windows(state):
        return [client for client in state["clients"] if not client["desktop"]]

    def wait_for(predicate, description, timeout=8):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                state = request()
                if predicate(state):
                    return state
            except (OSError, ValueError):
                pass
            time.sleep(0.1)
        raise AssertionError(description)

    with open(pathlib.Path(runtime) / "session.log", "w+") as log:
        process = subprocess.Popen(
            [
                str(binary),
                "--socket",
                "ludash-shell-test",
                "--graphics",
                "opengl",
                "--exit-after",
                "26000",
            ],
            env=env,
            stdout=log,
            stderr=log,
        )
        try:
            wait_for(
                lambda state: state["layerSurfaces"] >= 2 and state["shaderReady"],
                "Desktop did not map",
            )
            # The startup logo splash is also a layer surface, so the count above
            # can be reached while it still covers the screen. It closes on its own
            # fallback timer; wait until only the desktop remains so the first
            # clicks reach the panel instead of the splash.
            time.sleep(3.5)
            wait_for(
                lambda state: state["layerSurfaces"] == 2,
                "Startup splash stayed on screen",
            )
            window = subprocess.check_output(
                ["xdotool", "search", "--onlyvisible", "--pid", str(process.pid)],
                text=True,
            ).splitlines()[0]
            subprocess.run(["xdotool", "windowfocus", "--sync", window], check=True)

            def click(target):
                x, y = target
                subprocess.run(
                    [
                        "xdotool",
                        "mousemove",
                        "--window",
                        window,
                        str(x),
                        str(y),
                        "click",
                        "1",
                    ],
                    check=True,
                )

            # Workspace strip: the second workspace button switches desktops.
            click(WORKSPACE_2)
            wait_for(
                lambda state: state["workspace"] == 1,
                "Workspace button did not switch desktops",
            )

            # Launcher and settings are toggled by the centred selector.
            click(LAUNCHER)
            wait_for(lambda state: state["layerSurfaces"] >= 3, "Launcher did not open")
            click(LAUNCHER)
            wait_for(
                lambda state: state["layerSurfaces"] == 2, "Launcher did not close"
            )

            # Launching, minimizing and restoring a real window.
            assert "error" not in request("launch-default", "files")
            state = wait_for(
                lambda state: any(client["mapped"] for client in windows(state)),
                "Files did not open a window",
            )
            client = windows(state)[0]["id"]
            request("minimize", client)
            wait_for(
                lambda state: windows(state)[0]["minimized"],
                "Window did not minimize",
            )
            request("focus", client)
            wait_for(
                lambda state: not windows(state)[0]["minimized"],
                "Window did not restore",
            )

            click(SETTINGS)
            wait_for(lambda state: state["layerSurfaces"] >= 3, "Settings did not open")
            state = request()
            assert state["settingsPage"] == "general", state["settingsPage"]
            request("language", "zh_TW")
            wait_for(
                lambda state: state["language"] == "zh_TW",
                "Language did not update the compositor",
            )
            click(SETTINGS)
            wait_for(
                lambda state: state["layerSurfaces"] == 2, "Settings did not close"
            )
            request("language", "en_US")

            request("wallpaper", 1)
            wait_for(
                lambda state: not state["wallpaperImage"],
                "Shader wallpaper did not activate",
            )
            result = request("wallpaper-image", "/missing/ludash-wallpaper.png")
            assert result.get("error"), result
            request("wallpaper-default")
            wait_for(
                lambda state: bool(state["wallpaperImage"]),
                "Image wallpaper did not restore",
            )

            request("close", client)
            wait_for(lambda state: not windows(state), "Window did not close")
            assert process.wait(timeout=32) == 0, "Session did not close cleanly"
            print(
                "Shell interactions passed: panel workspace, launcher and settings "
                "clicks, window launch, minimize, restore, language and wallpaper."
            )
        except BaseException:
            log.flush()
            log.seek(0)
            print(log.read()[-12000:], file=sys.stderr)
            raise
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
