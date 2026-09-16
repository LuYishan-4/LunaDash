"""Verify the capture protocols and the session's own capture path."""

import json
import os
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from PIL import Image

build = Path(sys.argv[1]).resolve()
wayland_info = shutil.which("wayland-info")
xdotool = shutil.which("xdotool")
assert wayland_info, "wayland-info is required: sudo pacman -S --needed wayland-utils"
assert xdotool, "xdotool is required: sudo pacman -S --needed xdotool"

INTERFACE = re.compile(r"interface: '([^']+)',\s+version:\s+(\d+)")

with tempfile.TemporaryDirectory(prefix="ludash-capture-test-") as runtime:
    os.chmod(runtime, 0o700)
    env = os.environ | {
        "XDG_RUNTIME_DIR": runtime,
        "XDG_CONFIG_HOME": runtime,
        "QT_QPA_PLATFORM": "xcb",
        "QT_XCB_GL_INTEGRATION": "xcb_egl",
        "LIBGL_ALWAYS_SOFTWARE": "1",
        "LUDASH_SKIP_SETUP": "1",
        "QT_FORCE_STDERR_LOGGING": "1",
        "LC_ALL": "C.UTF-8",
    }
    for key in (
        "MESA_GL_VERSION_OVERRIDE",
        "MESA_GLSL_VERSION_OVERRIDE",
        "WAYLAND_DISPLAY",
    ):
        env.pop(key, None)
    socket_name = "ludash-capture-test"
    control = Path(runtime) / f"{socket_name}-control"
    shot = build / "screen-capture.png"
    state_path = build / "screen-capture-state.json"
    for stale in (shot, state_path):
        stale.unlink(missing_ok=True)

    def request(method="status", value=""):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(5)
            connection.connect(str(control))
            connection.sendall(
                json.dumps({"method": method, "value": value}).encode() + b"\n"
            )
            output = b""
            while b"\n" not in output:
                chunk = connection.recv(65536)
                if not chunk or len(output) > 1024 * 1024:
                    raise RuntimeError("Invalid IPC response")
                output += chunk
            return json.loads(output)

    with open(build / "screen-capture.log", "w+") as log:
        process = subprocess.Popen(
            [
                str(build / "lunadash-compositor"),
                "--no-shell",
                "--demo",
                "--graphics",
                "opengl",
                "--socket",
                socket_name,
                "--exit-after",
                "16000",
                "--state",
                str(state_path),
            ],
            env=env,
            stdout=log,
            stderr=log,
        )
        try:
            socket_file = Path(runtime) / socket_name
            deadline = time.monotonic() + 8
            while not socket_file.exists():
                assert process.poll() is None, (
                    "The compositor exited before creating its socket"
                )
                assert time.monotonic() < deadline, (
                    "The compositor did not create its Wayland socket"
                )
                time.sleep(0.1)
            status = request()
            deadline = time.monotonic() + 8
            while not status.get("clients"):
                assert time.monotonic() < deadline, "No demonstration client mapped"
                time.sleep(0.1)
                status = request()

            versions = {
                name: int(version)
                for name, version in INTERFACE.findall(
                    subprocess.run(
                        [wayland_info],
                        env=env | {"WAYLAND_DISPLAY": socket_name},
                        capture_output=True,
                        text=True,
                        timeout=20,
                        check=True,
                    ).stdout
                )
            }
            assert versions.get("zwlr_screencopy_manager_v1", 0) >= 2, versions
            assert versions.get("zxdg_output_manager_v1", 0) >= 1, versions
            assert versions.get("zwp_idle_inhibit_manager_v1", 0) >= 1, versions
            # Qt Wayland Compositor registers the wl_output global at version 2
            # (qwaylandoutput.cpp: d->init(display, 2)). grim, slurp and
            # wf-recorder bind wl_output above the advertised version and are
            # disconnected before they reach screencopy. When this assertion
            # starts failing, Qt advertises a newer wl_output and those tools can
            # be exercised here instead of the session capture path.
            assert versions.get("wl_output") == 2, versions

            assert "error" in request("capture", "relative.png")
            assert request("capture", str(shot)) == {"path": str(shot)}, shot
            assert shot.exists(), "The capture command wrote no file"
            assert "error" in request("capture", str(shot)), (
                "An existing file was overwritten"
            )
            status = request()
            frames = Image.open(shot).convert("RGB")
            assert frames.size == (
                status["display"]["width"],
                status["display"]["height"],
            ), status
            assert frames.getcolors(maxcolors=512) is None, (
                "The captured frame is blank"
            )
            assert (
                status["screenCapture"]["protocol"] == "zwlr_screencopy_manager_v1"
            ), status
            assert status["screenCapture"]["frames"] == 0, status["screenCapture"]
            assert status["activationEnvironment"] == {
                "published": False,
                "error": "",
            }, status

            # The shipped binding is what the session really matches: Alt+Shift+F5
            # arrives as a key event and is compared as portable text, so drive it
            # through the keyboard and wait for the capture the shortcut produces.
            assert status["shortcuts"]["screenshot"] == "Alt+Shift+F5", status[
                "shortcuts"
            ]
            window = subprocess.run(
                [xdotool, "search", "--name", "LunaDash Wayland"],
                env=env,
                capture_output=True,
                text=True,
                timeout=10,
                check=True,
            ).stdout.split()
            assert window, "The compositor window was not found"
            subprocess.run(
                [xdotool, "windowfocus", window[0]], env=env, check=True, timeout=10
            )
            subprocess.run(
                [xdotool, "key", "alt+shift+F5"], env=env, check=True, timeout=10
            )
            deadline = time.monotonic() + 5
            while not request()["screenCapture"]["lastCapture"]:
                assert time.monotonic() < deadline, (
                    "Alt+Shift+F5 did not capture the screen"
                )
                time.sleep(0.1)
            shortcut_shot = Path(request()["screenCapture"]["lastCapture"])
            assert shortcut_shot.exists(), shortcut_shot
            assert shortcut_shot.parent.name == "Screenshots", shortcut_shot
            assert Image.open(shortcut_shot).size == (
                status["display"]["width"],
                status["display"]["height"],
            ), shortcut_shot
            # The same action is reachable from the control socket.
            named_shot = Path(request("screenshot")["path"])
            assert named_shot.exists() and named_shot != shortcut_shot, named_shot

            exit_code = process.wait(timeout=24)
            # The session must terminate and must not be signalled. Its numeric
            # exit code also folds in child and XWayland cleanup timing, which
            # the full-session tests cover.
            assert exit_code >= 0, f"The capture session was signalled: {exit_code}"
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
print(
    "Screen capture passed: screencopy, xdg-output and idle-inhibit globals, plus the session capture path."
)
