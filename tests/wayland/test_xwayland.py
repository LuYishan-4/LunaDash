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
                        'WLR_BACKENDS': 'x11', 'WLR_X11_OUTPUTS': '1', 'WLR_RENDERER': 'pixman',
                        'LUDASH_SKIP_SETUP': '1', 'LUNADASH_DISABLE_FCITX': '1',
                        'QT_QPA_PLATFORMTHEME': 'generic', 'GTK_USE_PORTAL': '0',
                        'QT_QPA_PLATFORM': 'xcb', 'QT_XCB_GL_INTEGRATION': 'xcb_egl',
                        'LIBGL_ALWAYS_SOFTWARE': '1', 'QT_FORCE_STDERR_LOGGING': '1', 'LC_ALL': 'C.UTF-8'}
    for key in ('MESA_GL_VERSION_OVERRIDE', 'MESA_GLSL_VERSION_OVERRIDE', 'LUDASH_DISABLE_XWAYLAND'):
        env.pop(key, None)
    env.pop("WAYLAND_DISPLAY", None)
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
                                    '--exit-after', '30000'],
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

            # A Wayland application's native helper still needs authenticated
            # XOpenDisplay/XQueryExtension. No Discord binary or account is used.
            helper = Path(runtime) / 'discord'
            report = Path(runtime) / 'helper.json'
            helper.write_text("""#!/usr/bin/env python3
import ctypes
import json
import os
from pathlib import Path
import sys
x = ctypes.CDLL('libX11.so.6')
x.XOpenDisplay.argtypes = [ctypes.c_char_p]
x.XOpenDisplay.restype = ctypes.c_void_p
x.XQueryExtension.argtypes = [ctypes.c_void_p, ctypes.c_char_p] + [ctypes.POINTER(ctypes.c_int)] * 3
x.XCloseDisplay.argtypes = [ctypes.c_void_p]
display = x.XOpenDisplay(None)
assert display, 'Native input helper cannot open X display'
a, b, c = ctypes.c_int(), ctypes.c_int(), ctypes.c_int()
assert x.XQueryExtension(display, b'XInputExtension', ctypes.byref(a), ctypes.byref(b), ctypes.byref(c))
Path(__file__).with_name('helper.json').write_text(json.dumps({
    'display':os.environ.get('DISPLAY'), 'wayland':os.environ.get('WAYLAND_DISPLAY'),
    'authority':os.environ.get('XAUTHORITY'), 'arguments':sys.argv[1:]}))
x.XCloseDisplay(display)
""")
            helper.chmod(0o700)
            result = request('launch-command', json.dumps([str(helper)]))
            assert 'error' not in result, result
            deadline = time.monotonic() + 6
            while not report.exists():
                assert time.monotonic() < deadline, 'Native X11 input helper did not initialize'
                time.sleep(.05)
            native = json.loads(report.read_text())
            assert native['wayland'] == 'ludash-x11-test', native
            assert '--ozone-platform=wayland' in native['arguments'], native
            time.sleep(.3)
            state = request()
            assert state['xwayland']['running'] and not state['xwayland']['rootWindowVisible'], state
            assert not state['clients'] and not state['tiling']['groups'], 'Helper root leaked into the desktop/taskbar'

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
            assert display == native['display'] and str(authority) == native['authority'], 'Explicit X11 launch replaced the running helper server'
            assert state['xwayland']['rootWindowVisible'], state['xwayland']
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
            result = request('launch-x11', '"' + str(build / 'ludash-desktop') + '" --app console')
            assert 'error' not in result, result
            assert result['xwayland']['display'] == display and result['xwayland']['authority'] == str(authority), 'Repeated launches must reuse the authenticated display'
            request('quit')
            exit_code = process.wait(timeout=10)
            assert exit_code == 0, f'X11 session did not shut down cleanly: {exit_code}'
            assert not authority.exists(), 'Xauthority was not cleaned up'
            assert not Path('/tmp/.X11-unix/X' + display[1:]).exists(), 'Owned X11 socket was not cleaned up'
            print('X11 compatibility passed: native Wayland input helper, hidden helper root, reused server, cookie authentication, mapped X11 client and cleanup.')
        except BaseException:
            log.flush(); log.seek(0); print(log.read(), file=sys.stderr)
            raise
        finally:
            if process.poll() is None:
                process.terminate()
                try: process.wait(timeout=5)
                except subprocess.TimeoutExpired: process.kill(); process.wait(timeout=5)
