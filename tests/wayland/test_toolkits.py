"""Exercise actual GTK 3/4 and Qt clients against the selected wlroots renderer.

Default: headless pixman. For GPU verification set LUDASH_TEST_RENDERER=vulkan
and optionally LUDASH_TEST_HOST_WAYLAND=1. Unsupported Vulkan must fail, not skip.
"""
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time

build = Path(sys.argv[1]).resolve()
renderer = os.environ.get('LUDASH_TEST_RENDERER', 'pixman')
evidence = build / 'ci-evidence' / ('toolkits-' + renderer)
evidence.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='lunadash-toolkits-') as runtime:
    os.chmod(runtime, 0o700)
    env = os.environ | {
        'XDG_RUNTIME_DIR': runtime, 'XDG_CONFIG_HOME': runtime + '/config',
        'XDG_CACHE_HOME': runtime + '/cache', 'XDG_DATA_HOME': runtime + '/data',
        'WLR_BACKENDS': 'headless', 'WLR_HEADLESS_OUTPUTS': '1',
        'WLR_RENDERER': renderer, 'LUDASH_SKIP_SETUP': '1',
        'LUDASH_DISABLE_XWAYLAND': '1', 'QT_QUICK_BACKEND': 'software',
        'QT_QPA_PLATFORMTHEME': 'generic', 'LUDASH_LANGUAGE': 'en_US',
    }
    if os.environ.get('LUDASH_TEST_HOST_WAYLAND') == '1':
        display = Path(os.environ['WAYLAND_DISPLAY'])
        if not display.is_absolute():
            display = Path(os.environ['XDG_RUNTIME_DIR']) / display
        env.update(WLR_BACKENDS='wayland', WAYLAND_DISPLAY=str(display))
    control = runtime + '/lunadash-toolkits-control'
    def request(method='status', value=''):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(5)
            connection.connect(control)
            connection.sendall(json.dumps({'method': method, 'value': str(value)}).encode() + b'\n')
            response = b''
            while b'\n' not in response:
                chunk = connection.recv(65536)
                if not chunk or len(response) > 4 * 1024 * 1024:
                    raise RuntimeError('Invalid control response')
                response += chunk
            result = json.loads(response)
            assert 'error' not in result, result
            return result
    def wait_for(predicate, description):
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            if compositor.poll() is not None:
                raise AssertionError('Compositor exited before ' + description)
            try:
                state = request()
                if predicate(state):
                    return state
            except (OSError, ValueError):
                pass
            time.sleep(0.1)
        raise AssertionError('Timed out: ' + description)
    processes = []
    with (evidence / 'session.log').open('w+') as log:
        compositor = subprocess.Popen([str(build / 'lunadash-compositor'), '--no-shell',
            '--socket', 'lunadash-toolkits'], env=env, stdout=log, stderr=log)
        try:
            initial = wait_for(lambda state: state['display']['width'] > 0, 'output')
            assert initial['renderer'] == renderer, initial['renderer']
            assert 'translations' in initial
            lean = request('status', initial['language'])
            assert 'translations' not in lean
            assert 'translations' in request('status', 'stale-language')
            client_env = env | {'WAYLAND_DISPLAY': 'lunadash-toolkits', 'GDK_BACKEND': 'wayland',
                'QT_QPA_PLATFORM': 'wayland', 'GTK_A11Y': 'none', 'NO_AT_BRIDGE': '1',
                'LUNADASH_CONTROL': control}
            client_env.pop('DISPLAY', None)
            for version in ['3.0', '4.0']:
                processes.append(subprocess.Popen([sys.executable, str(Path(__file__).with_name('toolkit_client.py')), version],
                    env=client_env, stdout=log, stderr=log))
            processes.append(subprocess.Popen([str(build / 'lunadash-desktop'), '--app', 'welcome'],
                env=client_env, stdout=log, stderr=log))
            mapped = wait_for(lambda state: len([c for c in state['clients'] if c['mapped']]) == 3, 'GTK and Qt maps')
            (evidence / 'state.json').write_text(json.dumps(mapped, indent=2))
            titles = [c['title'] for c in mapped['clients']]
            assert 'LunaDash GTK 3.0' in titles and 'LunaDash GTK 4.0' in titles, titles
            assert 'LunaDash · Welcome' in titles, titles
            subprocess.run(['grim', str(evidence / 'toolkits.png')], env=client_env, check=True, timeout=15)
            for client in mapped['clients']:
                request('close', client['id'])
            wait_for(lambda state: not state['clients'], 'client destruction')
            for process in processes:
                assert process.wait(timeout=10) == 0, process.returncode
            request('quit', 'confirm')
            assert compositor.wait(timeout=10) == 0
            log.flush()
            output = (evidence / 'session.log').read_text()
            for error in ['ReferenceError:', 'TypeError:', 'Binding loop detected', 'Unable to assign', 'Welcome QML could not be loaded']:
                assert error not in output, output
        finally:
            for process in [*processes, compositor]:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait(timeout=5)
print('GTK 3, GTK 4 and Qt map/capture/close passed with ' + renderer)
