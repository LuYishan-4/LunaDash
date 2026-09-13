"""Graphics startup errors must exit with a diagnostic instead of SIGABRT."""
import os
import pathlib
import subprocess
import sys
import tempfile

binary = str(pathlib.Path(sys.argv[1]).resolve())
diagnostic_env = os.environ | {'QT_FORCE_STDERR_LOGGING': '1'}
for arguments in (['--graphics', 'invalid'], ['--graphics']):
    result = subprocess.run([binary, *arguments], env=diagnostic_env,
                            capture_output=True, text=True, timeout=5)
    assert result.returncode == 2, result
    assert '--graphics must be' in result.stderr, result.stderr

with tempfile.TemporaryDirectory(prefix='ludash-graphics-failure-') as runtime:
    os.chmod(runtime, 0o700)
    env = os.environ | {
        'XDG_RUNTIME_DIR': runtime, 'XDG_CONFIG_HOME': runtime,
        'QT_QPA_PLATFORM': 'xcb', 'QT_XCB_GL_INTEGRATION': 'xcb_egl',
        'LIBGL_ALWAYS_SOFTWARE': '1', 'MESA_GL_VERSION_OVERRIDE': '3.2',
        'QT_FORCE_STDERR_LOGGING': '1', 'QSG_RENDER_LOOP': 'threaded',
    }
    env.pop('MESA_GLSL_VERSION_OVERRIDE', None)
    result = subprocess.run([binary, '--no-shell', '--graphics', 'opengl',
                             '--exit-after', '1500'], env=env,
                            capture_output=True, text=True, timeout=12)
    assert result.returncode == 2, (result.returncode, result.stderr)
    assert 'LuDash graphics initialization failed:' in result.stderr, result.stderr
print('Graphics startup failures returned exit 2 without aborting.')
