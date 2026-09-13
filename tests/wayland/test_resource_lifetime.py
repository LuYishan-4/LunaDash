"""Detect sustained descriptor growth in the compositor and Quickshell."""
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time

root = Path(__file__).resolve().parents[2]
build = Path(sys.argv[1]).resolve()
duration = int(os.environ.get('LUDASH_SOAK_SECONDS', '45'))
assert 30 <= duration <= 180, 'LUDASH_SOAK_SECONDS must be between 30 and 180'
host = os.environ.get('LUDASH_TEST_HOST_WAYLAND') == '1'
outer = os.environ.get('WAYLAND_DISPLAY', '')
if host:
    assert outer, 'A running host Wayland session is required'
    if not outer.startswith('/'):
        outer = str(Path(os.environ['XDG_RUNTIME_DIR']) / outer)
    assert Path(outer).is_socket(), 'Host Wayland socket is unavailable'
evidence = build / 'ci-evidence'
evidence.mkdir(parents=True, exist_ok=True)
stem = 'resource-host' if host else 'resource-software'
samples = []


def descriptors(pid):
    try:
        entries = list((Path('/proc') / str(pid) / 'fd').iterdir())
    except FileNotFoundError:
        return None
    fences = 0
    for entry in entries:
        try:
            fences += str(entry.readlink()) == 'anon_inode:sync_file'
        except FileNotFoundError:
            pass  # A control helper can finish while the sample is taken.
    return {'fds': len(entries), 'syncFiles': fences}


with tempfile.TemporaryDirectory(prefix='ludash-resources-') as runtime:
    env = os.environ | {'XDG_RUNTIME_DIR': runtime, 'XDG_CONFIG_HOME': runtime,
                        'QT_QPA_PLATFORM': 'wayland' if host else 'xcb',
                        'QT_XCB_GL_INTEGRATION': 'xcb_egl', 'QT_FORCE_STDERR_LOGGING': '1',
                        'LUDASH_SKIP_SETUP': '1', 'LUDASH_LANGUAGE': 'en_US', 'LC_ALL': 'C.UTF-8'}
    for key in ('MESA_GL_VERSION_OVERRIDE', 'MESA_GLSL_VERSION_OVERRIDE'):
        env.pop(key, None)
    if host:
        env['WAYLAND_DISPLAY'] = outer
        env.pop('LIBGL_ALWAYS_SOFTWARE', None)
    else:
        env['LIBGL_ALWAYS_SOFTWARE'] = '1'
    control = runtime + '/ludash-resources-control'

    def request(method='status', value=''):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(3)
            connection.connect(control)
            connection.sendall(json.dumps({'method': method, 'value': value}).encode() + b'\n')
            data = b''
            while b'\n' not in data:
                chunk = connection.recv(65536)
                assert chunk and len(data) < 1024 * 1024, 'Invalid control response'
                data += chunk
            return json.loads(data)

    log_path = evidence / (stem + '.log')
    with log_path.open('w+') as log:
        process = subprocess.Popen([str(build / 'ludash-compositor'), '--socket', 'ludash-resources',
                                    '--graphics', os.environ.get('LUDASH_GRAPHICS', 'gles'), '--demo',
                                    '--exit-after', str((duration + 12) * 1000)], env=env, stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 10
            ready = False
            while time.monotonic() < deadline:
                assert process.poll() is None, 'Compositor stopped during startup'
                try:
                    state = request()
                    ready = state['shaderReady'] and state['layerSurfaces'] >= 2 and len(state['clients']) == 3
                    if ready and all(client['mapped'] for client in state['clients']):
                        break
                except (OSError, ValueError):
                    pass
                time.sleep(.1)
            else:
                raise AssertionError('Desktop did not become ready')
            child_ids = Path(f'/proc/{process.pid}/task/{process.pid}/children').read_text().split()
            shell_ids = [int(pid) for pid in child_ids if (Path('/proc') / pid / 'comm').read_text().strip() == 'quickshell']
            assert len(shell_ids) == 1, 'Expected one owned Quickshell process'
            watched = {'compositor': process.pid, 'shell': shell_ids[0]}
            start = time.monotonic()
            baseline = None
            tick = 0
            while time.monotonic() - start < duration:
                assert process.poll() is None, 'Compositor stopped during resource test'
                # Keep both scene graphs active instead of measuring an idle image.
                response = request('appearance', json.dumps({'accent': '#9ccbfb' if tick % 2 else '#c4b5fd'}))
                assert 'error' not in response and not response['processFailure'], response.get('error')
                current = {name: descriptors(pid) for name, pid in watched.items()}
                assert all(current.values()), 'A watched process disappeared'
                elapsed = round(time.monotonic() - start, 1)
                samples.append({'seconds': elapsed, 'processes': current})
                if elapsed >= 6:
                    if baseline is None:
                        baseline = current
                    for name, values in current.items():
                        assert values['fds'] - baseline[name]['fds'] <= 96, f'{name}: descriptors grew from {baseline[name]} to {values}'
                        assert values['syncFiles'] - baseline[name]['syncFiles'] <= 32, f'{name}: GPU fences grew from {baseline[name]} to {values}'
                tick += 1
                time.sleep(1)
            assert process.wait(timeout=18) == 0, 'Resource test did not close cleanly'
            subprocess.run([sys.executable, str(root / 'scripts/testing/check_graphics_log.py'), str(log_path)], check=True)
            print('Resource lifetime passed: active rendering, bounded descriptors/GPU fences and clean shutdown.')
        except BaseException:
            print(f'Resource evidence: {log_path}', file=sys.stderr)
            log.flush(); log.seek(0)
            print(log.read()[-12000:], file=sys.stderr)
            raise
        finally:
            (evidence / (stem + '.json')).write_text(json.dumps(samples, indent=2))
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill(); process.wait(timeout=5)
