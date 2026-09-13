"""Negative integration test: a crashed owned client must make the session fail."""
import os
import pathlib
import resource
import signal
import subprocess
import sys
import tempfile
import time

resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
binary = pathlib.Path(sys.argv[1]).resolve() / 'ludash-compositor'
with tempfile.TemporaryDirectory(prefix='ludash-crash-test-') as runtime:
    os.chmod(runtime, 0o700)
    env = os.environ | {'XDG_RUNTIME_DIR': runtime, 'XDG_CONFIG_HOME': runtime, 'QT_QPA_PLATFORM': 'xcb',
                        'QT_XCB_GL_INTEGRATION': 'xcb_egl', 'LIBGL_ALWAYS_SOFTWARE': '1'}
    env.pop('MESA_GL_VERSION_OVERRIDE', None)
    env.pop('MESA_GLSL_VERSION_OVERRIDE', None)
    with open(pathlib.Path(runtime) / 'output.log', 'w+') as output:
        process = subprocess.Popen([str(binary), '--no-shell', '--demo', '--exit-after', '4000'], env=env, stdout=output, stderr=output)
        try:
            children_file = pathlib.Path(f'/proc/{process.pid}/task/{process.pid}/children')
            deadline = time.monotonic() + 3
            child = None
            while time.monotonic() < deadline and process.poll() is None:
                for candidate in children_file.read_text().split():
                    try:
                        executable = pathlib.Path(f'/proc/{candidate}/exe').resolve()
                    except OSError:
                        continue
                    if executable.name == 'ludash-desktop':
                        child = int(candidate)
                        break
                if child:
                    break
                time.sleep(.05)
            assert child, 'No owned test client started'
            os.kill(child, signal.SIGSEGV)
            status = process.wait(timeout=12)
            assert status == 2, f'Client crash should fail the session with exit 2; got {status}'
            print('Crash detection passed: owned client crash caused test failure as expected.')
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=8)
