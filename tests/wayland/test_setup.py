"""Click through first-run setup and verify preferences survive a new session."""
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time

binary = Path(sys.argv[1]).resolve() / 'ludash-compositor'
with tempfile.TemporaryDirectory(prefix='ludash-setup-') as runtime:
    env = os.environ | {'XDG_RUNTIME_DIR': runtime, 'XDG_CONFIG_HOME': runtime,
                        'QT_QPA_PLATFORM': 'xcb', 'QT_XCB_GL_INTEGRATION': 'xcb_egl',
                        'LIBGL_ALWAYS_SOFTWARE': '1', 'QT_FORCE_STDERR_LOGGING': '1', 'LANG': 'C.UTF-8', 'LC_ALL': 'C.UTF-8'}
    for name in ('MESA_GL_VERSION_OVERRIDE', 'MESA_GLSL_VERSION_OVERRIDE', 'LUDASH_SKIP_SETUP', 'LUDASH_LANGUAGE'):
        env.pop(name, None)
    control = runtime + '/ludash-setup-control'

    def request(method='status', value=''):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(2)
            connection.connect(control)
            connection.sendall(json.dumps({'method': method, 'value': value}).encode() + b'\n')
            output = b''
            while b'\n' not in output:
                chunk = connection.recv(65536)
                if not chunk or len(output) > 1024 * 1024:
                    raise RuntimeError('Invalid control response')
                output += chunk
            return json.loads(output)

    def wait_for(predicate):
        deadline = time.monotonic() + 6
        while time.monotonic() < deadline:
            try:
                result = request()
                if predicate(result):
                    return result
            except (OSError, ValueError):
                pass
            time.sleep(.1)
        raise AssertionError('Setup state did not match expectation')

    for run in range(2):
        with open(Path(runtime) / f'run-{run}.log', 'w+') as log:
            process = subprocess.Popen([str(binary), '--socket', 'ludash-setup', '--graphics', 'opengl',
                                        '--exit-after', '13000' if run == 0 else '4000'], env=env, stdout=log, stderr=log)
            try:
                state = wait_for(lambda data: data['layerSurfaces'] >= 2 and data['shaderReady'])
                if run == 0:
                    assert not state['setupComplete'], state
                    wait_for(lambda data: data['layerSurfaces'] >= 3)
                    window = subprocess.check_output(['xdotool', 'search', '--onlyvisible', '--pid', str(process.pid)], text=True).splitlines()[0]
                    subprocess.run(['xdotool', 'windowfocus', '--sync', window], check=True)

                    def click(x, y):
                        subprocess.run(['xdotool', 'mousemove', '--window', window, str(x), str(y), 'click', '1'], check=True)
                        time.sleep(.35)

                    click(980, 672)  # Language -> network; never changes host connections.
                    click(980, 672)  # Continue offline -> appearance.
                    click(546, 352)  # Lavender preset.
                    wait_for(lambda data: data['appearance']['accent'] == '#c4b5fd')
                    click(980, 672)  # Appearance -> ready.
                    click(980, 672)  # Complete the guide.
                    wait_for(lambda data: data['setupComplete'] and data['layerSurfaces'] == 2)
                    assert 'error' not in request('appearance', json.dumps({'gap': 20, 'panelHeight': 36})), 'Valid preferences rejected'
                    before = request()['appearance']
                    assert 'error' in request('appearance', '{"accent":"#123456","gap":99}')
                    assert request()['appearance'] == before, 'Invalid update partially changed preferences'
                else:
                    assert state['setupComplete'], 'Guide completion was not persisted'
                    assert state['appearance']['accent'] == '#c4b5fd', state
                    assert state['appearance']['gap'] == 20 and state['appearance']['panelHeight'] == 36, state
                    assert state['layerSurfaces'] == 2, state
                assert process.wait(timeout=20) == 0, 'Setup session did not close cleanly'
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
    print('First-run setup passed: offline flow, appearance clicks, invalid update rejection and persisted restart.')
