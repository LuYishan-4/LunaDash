"""Verify an authenticated X11 app becomes a tiled Wayland client."""
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
with tempfile.TemporaryDirectory(prefix='ludash-x11-test-') as runtime:
    env = os.environ | {'XDG_RUNTIME_DIR': runtime, 'XDG_CONFIG_HOME': runtime,
                        'QT_QPA_PLATFORM': 'xcb', 'QT_XCB_GL_INTEGRATION': 'xcb_egl',
                        'LIBGL_ALWAYS_SOFTWARE': '1', 'QT_FORCE_STDERR_LOGGING': '1', 'LC_ALL': 'C.UTF-8'}
    for key in ('MESA_GL_VERSION_OVERRIDE', 'MESA_GLSL_VERSION_OVERRIDE', 'LUDASH_DISABLE_XWAYLAND'):
        env.pop(key, None)
    control = runtime + '/ludash-x11-test-control'

    def request(method='status', value=''):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(3)
            connection.connect(control)
            connection.sendall(json.dumps({'method': method, 'value': value}).encode() + b'\n')
            output = b''
            while b'\n' not in output:
                chunk = connection.recv(65536)
                if not chunk or len(output) > 1024 * 1024:
                    raise RuntimeError('Invalid IPC response')
                output += chunk
            return json.loads(output)

    with open(build / 'xwayland.log', 'w+') as log:
        process = subprocess.Popen([str(build / 'ludash-compositor'), '--no-shell', '--socket', 'ludash-x11-test',
                                    '--exit-after', '16000'],
                                   env=env, stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 8
            while True:
                try:
                    state = request()
                    if state['xwayland']['available']:
                        break
                except (OSError, ValueError):
                    pass
                assert time.monotonic() < deadline, 'XWayland did not start'
                time.sleep(.1)
            assert not state['xwayland']['running'], 'XWayland must not open a rootful desktop window at session startup'
            assert not state['xwayland']['display'], state['xwayland']
            assert not state['xwayland']['authority'], state['xwayland']

            result = request('launch-x11', '"' + str(build / 'ludash-desktop') + '" --app console')
            assert 'error' not in result, result
            deadline = time.monotonic() + 4
            while True:
                state = request()
                if state['xwayland']['running']:
                    break
                assert time.monotonic() < deadline, 'XWayland did not start on demand'
                time.sleep(.05)
            display = state['xwayland']['display']
            authority = Path(state['xwayland']['authority'])
            assert display.startswith(':'), state['xwayland']
            assert authority.stat().st_mode & 0o077 == 0, 'Xauthority is not owner-only'
            with socket.socket(socket.AF_UNIX) as connection:
                connection.settimeout(6)
                connection.connect('/tmp/.X11-unix/X' + display[1:])
                connection.sendall(b'l\0' + struct.pack('<HHHHH', 11, 0, 0, 0, 0))
                assert connection.recv(8)[0] == 0, 'Unauthenticated X11 connection was accepted'
            deadline = time.monotonic() + 6
            while True:
                state = request()
                if any(client['mapped'] and client['bufferWidth'] > 0 for client in state['clients']) and subprocess.run(['xdotool', 'search', '--name', 'console'], env=env | {'DISPLAY': display, 'XAUTHORITY': str(authority)}, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=3).returncode == 0:
                    break
                assert time.monotonic() < deadline, 'X11 client did not become a Wayland window'
                time.sleep(.1)
            assert state['appearance']['blur'] and not state['blurFailed'], state
            exit_code = process.wait(timeout=22)
            assert exit_code == 0, f'X11 session did not shut down cleanly: {exit_code}'
            assert not authority.exists(), 'Xauthority was not cleaned up'
            assert not Path('/tmp/.X11-unix/X' + display[1:]).exists(), 'Owned X11 socket was not cleaned up'
            print('X11 compatibility passed: cookie authentication, denied unauthenticated access, mapped client and cleanup.')
        except BaseException:
            log.flush(); log.seek(0); print(log.read(), file=sys.stderr)
            raise
        finally:
            if process.poll() is None:
                process.terminate()
                try: process.wait(timeout=5)
                except subprocess.TimeoutExpired: process.kill(); process.wait(timeout=5)
