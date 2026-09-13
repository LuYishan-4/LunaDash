"""Verify module fallback, real default apps, Fish and live Files colors in isolation."""
import json
import os
from pathlib import Path
import shlex
import socket
import subprocess
import sys
import tempfile
import time
from PIL import ImageGrab

build = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='ludash-customization-') as runtime:
    env = os.environ | {'XDG_RUNTIME_DIR': runtime, 'XDG_CONFIG_HOME': runtime, 'XDG_DATA_HOME': runtime + '/data', 'XDG_CACHE_HOME': runtime + '/cache', 'QT_QPA_PLATFORM': 'xcb',
                        'QT_XCB_GL_INTEGRATION': 'xcb_egl', 'LIBGL_ALWAYS_SOFTWARE': '1', 'LUDASH_SKIP_SETUP': '1',
                        'LUDASH_LANGUAGE': 'en_US', 'QT_FORCE_STDERR_LOGGING': '1'}
    for key in ('MESA_GL_VERSION_OVERRIDE', 'MESA_GLSL_VERSION_OVERRIDE'):
        env.pop(key, None)
    def request(method='status', value=''):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(3); connection.connect(runtime + '/ludash-custom-control')
            connection.sendall(json.dumps({'method': method, 'value': str(value)}).encode() + b'\n')
            data = b''
            while b'\n' not in data:
                chunk = connection.recv(65536)
                if not chunk or len(data) > 1024 * 1024: raise RuntimeError('Invalid IPC response')
                data += chunk
            return json.loads(data)
    def wait_for(predicate):
        deadline = time.monotonic() + 7
        while time.monotonic() < deadline:
            try:
                state = request()
                if predicate(state): return state
            except OSError: pass
            time.sleep(.12)
        raise AssertionError('Customization state did not match expectation')
    with open(build / 'customization.log', 'w+') as log:
        process = subprocess.Popen([str(build / 'ludash-compositor'), '--socket', 'ludash-custom', '--exit-after', '34000'], env=env, stdout=log, stderr=log)
        try:
            state = wait_for(lambda s: s['layerSurfaces'] >= 2)
            assert not state['shellModules']['trusted']
            assert 'error' not in request('module-template', 'panel')
            document = {'schemaVersion': 1, 'modules': {'panel': {'style': {'height': 48, 'margin': 8, 'radius': 22, 'edge': 'bottom'}, 'custom': {'enabled': True, 'entry': 'panel/Main.qml'}}}}
            state = request('module-save', json.dumps(document)); assert 'error' not in state, state
            assert state['panelExtent'] == 64 and state['panelAtBottom']
            assert not state['shellModules']['modules']['panel']['custom']['source']
            assert 'error' in request('module-save', '{"schemaVersion":1,"modules":{"settings":{"enabled":false}}}')
            assert request()['panelExtent'] == 64
            request('module-code-trust', 'true'); time.sleep(1.5)
            assert not request()['shellModules']['errors']
            # A template without syntax errors really replaces the default content.
            window = subprocess.check_output(['xdotool', 'search', '--onlyvisible', '--pid', str(process.pid)], text=True).splitlines()[0]
            subprocess.run(['xdotool', 'windowfocus', '--sync', window], check=True)
            subprocess.run(['xdotool', 'mousemove', '--window', window, '1380', '868', 'click', '1'], check=True)
            wait_for(lambda s: s['layerSurfaces'] >= 3)
            path = Path(state['shellModules']['codeRoot']) / 'panel/Main.qml'
            path.write_text('import QtQuick\nItem { this is invalid QML }\n')
            wait_for(lambda s: 'panel' in s['shellModules']['errors'])
            assert request()['layerSurfaces'] >= 3, 'Settings lost during fallback'
            request('module-reset'); wait_for(lambda s: s['panelExtent'] == 40 and not s['shellModules']['trusted'])
            # Dismiss settings with its normal close control.
            time.sleep(1)
            subprocess.run(['xdotool', 'mousemove', '--window', window, '1218', '98', 'click', '1'], check=True)
            wait_for(lambda s: s['layerSurfaces'] == 2)
            assert 'error' not in request('launch-default', 'files')
            state = wait_for(lambda s: any(c['mapped'] and 'files' in c['title'] for c in s['clients']))
            client = next(c for c in state['clients'] if 'files' in c['title'])
            demo = Path(runtime) / 'Workspace'
            demo.mkdir()
            for name in ('Design', 'Documents', 'Music', 'Pictures', 'Projects', 'Wallpapers'):
                (demo / name).mkdir()
            (demo / 'Welcome.md').write_text('# Your workspace\nExample files for the LuDash preview.\n')
            subprocess.run(['xdotool', 'key', 'ctrl+l'], check=True)
            subprocess.run(['xdotool', 'type', '--clearmodifiers', str(demo)], check=True)
            subprocess.run(['xdotool', 'key', 'Return'], check=True)
            time.sleep(1)
            before = ImageGrab.grab().convert('RGB')
            request('appearance', '{"accent":"#dfa5bd"}')
            time.sleep(1.6)
            after = ImageGrab.grab().convert('RGB'); after.save(build / 'files-preview.png')
            # Compare inside the app, excluding its compositor frame.
            x, y, w, h = (int(client[k]) for k in ('x', 'y', 'width', 'height'))
            crop = (x + 20, y + 60, x + w - 20, y + h - 20)
            assert before.crop(crop).tobytes() != after.crop(crop).tobytes(), 'Files did not recolor'
            request('close', client['id']); wait_for(lambda s: not s['clients'])
            result = request('launch-default', 'terminal'); assert 'error' not in result, result
            terminal = wait_for(lambda s: any(c['mapped'] and 'LuDash Terminal' in c['title'] for c in s['clients']))
            request('focus', terminal['clients'][0]['id'])
            time.sleep(.7)
            subprocess.run(['xdotool', 'mousemove', '--window', window, '720', '450', 'click', '1'], check=True)
            marker = Path(runtime) / 'fish-version'
            command = 'printf "%s" $version > ' + shlex.quote(str(marker))
            subprocess.run(['xdotool', 'type', '--clearmodifiers', '--delay', '2', command], check=True)
            subprocess.run(['xdotool', 'key', 'Return'], check=True)
            deadline = time.monotonic() + 4
            while time.monotonic() < deadline and (not marker.exists() or not marker.read_text().strip()): time.sleep(.1)
            assert marker.exists() and marker.read_text().strip(), 'Interactive Fish did not execute the command'
            subprocess.run(['xdotool', 'type', '--clearmodifiers', 'exit'], check=True)
            subprocess.run(['xdotool', 'key', 'Return'], check=True)
            wait_for(lambda s: not s['clients'])
            assert process.wait(timeout=32) == 0, 'Customization session did not close cleanly'
            print('Customization passed: JSON validation, template replacement, error fallback, reset, live Files palette and interactive Fish.')
        except BaseException:
            ImageGrab.grab().save(build / 'customization-failure.png')
            log.flush(); log.seek(0); print(log.read(), file=sys.stderr); raise
        finally:
            if process.poll() is None:
                process.terminate()
                try: process.wait(timeout=5)
                except subprocess.TimeoutExpired: process.kill(); process.wait(timeout=5)
