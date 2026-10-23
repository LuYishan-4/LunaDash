"""Exercise GTK 3/4, Qt and Slint clients against the selected wlroots renderer.

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
            assert initial['display']['refreshRate'] > 0, initial['display']
            assert 'translations' in initial
            lean = request('status', initial['language'])
            assert 'translations' not in lean
            assert 'translations' in request('status', 'stale-language')
            client_env = env | {'WAYLAND_DISPLAY': 'lunadash-toolkits', 'GDK_BACKEND': 'wayland',
                'QT_QPA_PLATFORM': 'wayland', 'GTK_A11Y': 'none', 'NO_AT_BRIDGE': '1',
                'LUNADASH_CONTROL': control}
            client_env.pop('DISPLAY', None)
            for version in ['3.0', '4.0']:
                processes.append(subprocess.Popen([sys.executable,
                    str(Path(__file__).with_name('toolkit_client.py')), version],
                    env=client_env, stdout=log, stderr=log))
            processes.append(subprocess.Popen([str(build / 'lunadash-qt-toolkit-client')],
                env=client_env, stdout=log, stderr=log))
            processes.append(subprocess.Popen([str(build / 'lunadash-desktop'), '--app', 'welcome'],
                env=client_env, stdout=log, stderr=log))
            welcome = processes[-1]
            chooser = subprocess.Popen([str(build / 'lunadash-screencast-chooser')],
                env=client_env, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=log, text=True)
            chooser.stdin.write('Monitor: Headless test output\nWindow: LunaDash GTK 3.0\n')
            chooser.stdin.close()
            processes.append(chooser)
            qt_client = processes[-3]
            packages = subprocess.Popen([str(build / 'lunadash-desktop'), '--app', 'packages'],
                env=client_env, stdout=log, stderr=log)
            processes.append(packages)
            mapped = wait_for(lambda state: len([c for c in state['clients'] if c['mapped']]) == 6,
                'GTK, Qt, Welcome, Package Manager and ScreenCast clients')
            welcome_maps = Path(f'/proc/{welcome.pid}/maps').read_text()
            qt_maps = Path(f'/proc/{qt_client.pid}/maps').read_text()
            chooser_maps = Path(f'/proc/{chooser.pid}/maps').read_text()
            packages_maps = Path(f'/proc/{packages.pid}/maps').read_text()
            assert 'libQt6' not in welcome_maps, 'The standalone Slint Welcome process loaded Qt'
            assert 'libQt6' not in chooser_maps, 'The standalone Slint ScreenCast chooser loaded Qt'
            assert 'libQt6' not in packages_maps, 'The standalone Slint Package Manager process loaded Qt'
            assert 'libQt6' in qt_maps, 'The Qt compatibility client did not load Qt'
            (evidence / 'state.json').write_text(json.dumps(mapped, indent=2))
            titles = [c['title'] for c in mapped['clients']]
            assert 'LunaDash GTK 3.0' in titles and 'LunaDash GTK 4.0' in titles, titles
            assert 'LunaDash Qt 6' in titles, titles
            assert 'LunaDash · Welcome' in titles, titles
            assert 'LunaDash · Package manager' in titles, titles
            assert 'Share your screen' in titles, titles
            chooser_client = next(c for c in mapped['clients'] if c['title'] == 'Share your screen')
            subprocess.run(['grim', str(evidence / 'toolkits.png')], env=client_env, check=True, timeout=15)
            request('close', chooser_client['id'])
            for client in mapped['clients']:
                if client['id'] == chooser_client['id']:
                    continue
                request('close', client['id'])
            wait_for(lambda state: not state['clients'], 'client destruction')
            assert chooser.wait(timeout=10) == 0, 'Slint ScreenCast chooser did not close cleanly'
            assert chooser.stdout.read() == '', 'Closing the chooser must not select a source'
            process_names = ['GTK 3', 'GTK 4', 'Qt 6', 'Slint Welcome', 'Slint ScreenCast chooser', 'Slint Package Manager']
            for name, process in zip(process_names, processes):
                returncode = process.wait(timeout=10)
                assert returncode == 0, f'{name} client {process.pid} exited with {returncode}'
            package_window = subprocess.Popen([str(build / 'lunadash-desktop'), '--app', 'packages'],
                env=client_env, stdout=log, stderr=log)
            processes.append(package_window)
            package_state = wait_for(lambda state: any(
                client['mapped'] and client['title'] == 'LunaDash · Package manager'
                for client in state['clients']), 'standalone Package Manager window')
            assert 'libQt6' not in Path(f'/proc/{package_window.pid}/maps').read_text(), \
                'The standalone Slint Package Manager process loaded Qt'
            subprocess.run(['grim', str(evidence / 'packages.png')], env=client_env, check=True, timeout=15)
            package_client = next(c for c in package_state['clients']
                if c['title'] == 'LunaDash · Package manager')
            request('close', package_client['id'])
            assert package_window.wait(timeout=10) == 0, 'Slint Package Manager did not close cleanly'
            wait_for(lambda state: not state['clients'], 'standalone Package Manager destruction')
            request('quit', 'confirm')
            assert compositor.wait(timeout=10) == 0
            log.flush()
            output = (evidence / 'session.log').read_text()
            for error in ['Slint error:', 'LunaDash Welcome could not start:']:
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
print('GTK 3, GTK 4, Qt and standalone Slint clients mapped/captured/closed with ' + renderer)
