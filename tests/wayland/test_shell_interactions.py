"""Exercise Quickshell clicks and IPC in an isolated nested Wayland session."""
import json
import os
import pathlib
import socket
import subprocess
import sys
import tempfile
import time

binary = pathlib.Path(sys.argv[1]).resolve() / 'ludash-compositor'
with tempfile.TemporaryDirectory(prefix='ludash-shell-test-') as runtime:
    env = os.environ | {
        'XDG_RUNTIME_DIR': runtime, 'XDG_CONFIG_HOME': runtime,
        'QT_QPA_PLATFORM': 'xcb', 'QT_XCB_GL_INTEGRATION': 'xcb_egl',
        'LUDASH_SKIP_SETUP': '1', 'LIBGL_ALWAYS_SOFTWARE': '1', 'QT_FORCE_STDERR_LOGGING': '1', 'LANG': 'C.UTF-8', 'LC_ALL': 'C.UTF-8',
    }
    for name in ('MESA_GL_VERSION_OVERRIDE', 'MESA_GLSL_VERSION_OVERRIDE', 'LUDASH_LANGUAGE'):
        env.pop(name, None)
    control = runtime + '/ludash-shell-test-control'

    def request(method='status', value=''):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(2)
            connection.connect(control)
            connection.sendall(json.dumps({'method': method, 'value': str(value)}).encode() + b'\n')
            output = b''
            while b'\n' not in output:
                chunk = connection.recv(65536)
                if not chunk:
                    raise RuntimeError('Control connection closed without a response')
                output += chunk
                if len(output) > 1024 * 1024:
                    raise RuntimeError('Control response exceeded its limit')
            return json.loads(output)

    def wait_for(predicate, description):
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            try:
                state = request()
                if predicate(state):
                    return state
            except (OSError, ValueError):
                pass
            time.sleep(.1)
        raise AssertionError(description)

    with open(pathlib.Path(runtime) / 'session.log', 'w+') as log:
        process = subprocess.Popen([str(binary), '--socket', 'ludash-shell-test',
                                    '--graphics', 'opengl', '--exit-after', '22000'],
                                   env=env, stdout=log, stderr=log)
        try:
            wait_for(lambda state: state['layerSurfaces'] >= 3 and state['shaderReady'], 'Desktop did not map')
            window = subprocess.check_output(['xdotool', 'search', '--onlyvisible', '--pid', str(process.pid)], text=True).splitlines()[0]
            subprocess.run(['xdotool', 'windowfocus', '--sync', window], check=True)

            def click(x, y):
                subprocess.run(['xdotool', 'mousemove', '--window', window, str(x), str(y), 'click', '1'], check=True)

            click(70, 14)
            wait_for(lambda state: state['workspace'] == 1, 'Workspace button did not switch desktops')
            click(18, 14)
            wait_for(lambda state: state['layerSurfaces'] >= 4, 'Launcher did not open')
            time.sleep(.3)
            click(135, 120)
            state = wait_for(lambda state: any(client['mapped'] for client in state['clients']), 'Files launcher button did not open a client')
            client = state['clients'][0]['id']
            request('minimize', client)
            wait_for(lambda state: not state['clients'][0]['visible'], 'Client did not minimize')
            request('focus', client)
            wait_for(lambda state: state['clients'][0]['visible'], 'Client did not restore')

            click(1390, 14)
            wait_for(lambda state: state['layerSurfaces'] >= 4, 'Settings did not open')
            time.sleep(.3)
            click(1010, 145)
            wait_for(lambda state: state['language'] == 'zh_TW', 'Language button did not update the compositor')
            click(1390, 14)
            request('wallpaper', 1)
            wait_for(lambda state: not state['wallpaperImage'], 'Shader wallpaper did not activate')
            result = request('wallpaper-image', '/missing/ludash-wallpaper.png')
            assert result.get('error'), result
            request('wallpaper-default')
            wait_for(lambda state: bool(state['wallpaperImage']), 'Image wallpaper did not restore')
            request('close', client)
            wait_for(lambda state: not state['clients'], 'Client did not close')
            assert process.wait(timeout=25) == 0, 'Session did not close cleanly'
            print('Shell interactions passed: workspace, launcher, settings, language, wallpaper, minimize and restore.')
        except BaseException:
            log.flush(); log.seek(0); print(log.read(), file=sys.stderr)
            raise
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill(); process.wait(timeout=5)
