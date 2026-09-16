"""Verify that a numeric keypad key does not drop a client's lock modifiers."""

import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

build = Path(sys.argv[1]).resolve()
konsole = shutil.which("konsole")
xdotool = shutil.which("xdotool")
assert konsole, "konsole is required for this test"
assert xdotool, "xdotool is required: sudo pacman -S --needed xdotool"

with tempfile.TemporaryDirectory(prefix="ludash-locks-test-") as runtime:
    os.chmod(runtime, 0o700)
    home = Path(runtime) / "home"
    home.mkdir()
    typed = Path(runtime) / "typed.txt"
    env = os.environ | {
        "XDG_RUNTIME_DIR": runtime,
        "XDG_CONFIG_HOME": runtime,
        "XDG_DATA_HOME": str(Path(runtime) / "data"),
        "XDG_CACHE_HOME": str(Path(runtime) / "cache"),
        "HOME": str(home),
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
    socket_name = "ludash-locks-test"
    control = Path(runtime) / f"{socket_name}-control"

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
                if not chunk:
                    raise RuntimeError("Invalid IPC response")
                output += chunk
            return json.loads(output)

    def wait_for(predicate, message, timeout=12):
        deadline = time.monotonic() + timeout
        while True:
            state = request()
            if predicate(state):
                return state
            assert time.monotonic() < deadline, message
            time.sleep(0.1)

    with open(build / "keyboard-locks.log", "w+") as log:
        process = subprocess.Popen(
            [
                str(build / "lunadash-compositor"),
                "--no-shell",
                "--graphics",
                "opengl",
                "--socket",
                socket_name,
                "--exit-after",
                "30000",
            ],
            env=env,
            stdout=log,
            stderr=log,
        )
        terminal = None
        try:
            socket_file = Path(runtime) / socket_name
            deadline = time.monotonic() + 8
            while not socket_file.exists():
                assert process.poll() is None, "The compositor exited early"
                assert time.monotonic() < deadline, "The compositor created no socket"
                time.sleep(0.1)
            # Konsole is a native Wayland client; kitty is routed to XWayland.
            # The toolkit variables are overridden because this harness runs under
            # Xvfb, where an inherited xcb backend would bypass the compositor.
            terminal_env = env | {
                "WAYLAND_DISPLAY": socket_name,
                "QT_QPA_PLATFORM": "wayland",
            }
            for key in ("DISPLAY", "XAUTHORITY"):
                terminal_env.pop(key, None)
            terminal = subprocess.Popen(
                [konsole, "-e", "sh", "-c", f"cat > {typed}"],
                env=terminal_env,
                stdout=log,
                stderr=log,
            )
            wait_for(
                lambda s: any(c["mapped"] for c in s["clients"]),
                "The terminal did not map",
            )
            window = subprocess.check_output(
                [xdotool, "search", "--onlyvisible", "--pid", str(process.pid)],
                text=True,
                timeout=10,
            ).splitlines()[0]
            subprocess.run([xdotool, "windowfocus", "--sync", window], check=True)
            # Click inside the client itself: a click on the background takes the
            # keyboard focus away again in a scrolling tiling layout.
            state = request()
            client = next(c for c in state["clients"] if c["mapped"])
            inside_x = int(client["x"] + client["width"] / 2)
            inside_y = int(client["y"] + client["height"] / 2)
            subprocess.run(
                [
                    xdotool,
                    "mousemove",
                    "--window",
                    window,
                    str(inside_x),
                    str(inside_y),
                    "click",
                    "1",
                ],
                check=True,
            )
            time.sleep(1.5)

            # CapsLock reaches the client through its own xkb state.
            subprocess.run([xdotool, "key", "Caps_Lock"], check=True)
            time.sleep(0.6)
            subprocess.run([xdotool, "key", "a"], check=True)
            time.sleep(0.6)
            before = request()["input"]["keypadKeyForwards"]

            # A numeric keypad key makes Qt Wayland Compositor send a modifiers
            # event with a zeroed locked mask. Keypad End is this layout's numeric
            # keypad 1 key while NumLock is off, so xdotool does not need to press
            # NumLock first and Qt sends no other modifier update afterwards.
            subprocess.run([xdotool, "key", "KP_End"], check=True)
            time.sleep(0.8)
            after = request()["input"]["keypadKeyForwards"]
            subprocess.run([xdotool, "key", "a"], check=True)
            time.sleep(0.6)
            # The terminal is in canonical mode, so cat writes the line only when
            # it receives a newline.
            subprocess.run([xdotool, "key", "Return"], check=True)
            time.sleep(1)

            text = typed.read_text(errors="replace")
            assert text.count("A") == 2, (
                f"A keypad key cleared the client's CapsLock: {text!r}"
            )
            assert after > before, (
                "The keypad key was not forwarded from its scan code: "
                f"{before} -> {after}"
            )
            exit_code = process.wait(timeout=35)
            assert exit_code >= 0, f"The session was signalled: {exit_code}"
        except BaseException:
            log.flush()
            log.seek(0)
            print(log.read(), file=sys.stderr)
            raise
        finally:
            if terminal and terminal.poll() is None:
                terminal.terminate()
                try:
                    terminal.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    terminal.kill()
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
print("Keyboard locks passed: a keypad key keeps the client's lock modifiers.")
