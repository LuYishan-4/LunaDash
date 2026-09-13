"""Exercise live blur/motion preferences and capture a private-safe native window."""
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time

build = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='ludash-effects-') as runtime:
    env = os.environ | {'XDG_RUNTIME_DIR': runtime, 'XDG_CONFIG_HOME': runtime,
                        'QT_QPA_PLATFORM': 'xcb', 'QT_XCB_GL_INTEGRATION': 'xcb_egl',
                        'LIBGL_ALWAYS_SOFTWARE': '1', 'LUDASH_SKIP_SETUP': '1',
                        'LUDASH_LANGUAGE': 'en_US', 'QT_FORCE_STDERR_LOGGING': '1'}
    for key in ('MESA_GL_VERSION_OVERRIDE', 'MESA_GLSL_VERSION_OVERRIDE'):
        env.pop(key, None)
    control = runtime + '/ludash-effects-control'

    def request(method='status', value=''):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(3)
            connection.connect(control)
            connection.sendall(json.dumps({'method': method, 'value': str(value)}).encode() + b'\n')
            result = b''
            while b'\n' not in result:
                chunk = connection.recv(65536)
                if not chunk or len(result) > 1024 * 1024:
                    raise RuntimeError('Invalid IPC response')
                result += chunk
            return json.loads(result)

    def wait_for(predicate):
        deadline = time.monotonic() + 6
        while time.monotonic() < deadline:
            try:
                state = request()
                if predicate(state):
                    return state
            except (OSError, ValueError):
                pass
            time.sleep(.08)
        raise AssertionError('Effects state did not match expectation')

    child = None
    with open(build / 'effects.log', 'w+') as log:
        process = subprocess.Popen([str(build / 'ludash-compositor'), '--socket', 'ludash-effects',
                                    '--exit-after', '11000', '--screenshot', str(build / 'blur-preview.png')],
                                   env=env, stdout=log, stderr=log)
        try:
            wait_for(lambda state: state['layerSurfaces'] >= 2)
            child = subprocess.Popen([str(build / 'ludash-desktop'), '--app', 'console'],
                                     env=env | {'QT_QPA_PLATFORM': 'wayland', 'WAYLAND_DISPLAY': 'ludash-effects'},
                                     stdout=log, stderr=log)
            state = wait_for(lambda state: state['blurReady'] and len(state['clients']) == 1)
            client = state['clients'][0]['id']
            assert 'error' not in request('appearance', json.dumps({'blur': False, 'animations': False, 'windowOpacity': 80}))
            request('minimize', client)
            wait_for(lambda state: not state['clients'][0]['visible'] and state['activeAnimations'] == 0)
            request('focus', client)
            wait_for(lambda state: state['clients'][0]['visible'] and state['activeAnimations'] == 0)
            assert 'error' not in request('appearance', json.dumps({'blur': True, 'blurRadius': 18, 'windowOpacity': 96, 'animations': True, 'animationDuration': 220}))
            wait_for(lambda state: state['blurReady'] and not state['blurFailed'])
            assert process.wait(timeout=18) == 0, 'Effects session did not close cleanly'
            assert child.wait(timeout=3) == 0, 'Native client did not close cleanly'
            print('Effects passed: live blur/opacity, reduced motion, minimize/restore and clean shutdown.')
        except BaseException:
            log.flush(); log.seek(0); print(log.read(), file=sys.stderr)
            raise
        finally:
            for owned in (child, process):
                if owned and owned.poll() is None:
                    owned.terminate()
                    try: owned.wait(timeout=5)
                    except subprocess.TimeoutExpired: owned.kill(); owned.wait(timeout=5)
