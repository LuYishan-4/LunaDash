"""Exercise login-script boundaries without acquiring a GPU or changing services."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
installer = root / 'scripts/install-session.sh'
assert subprocess.run([str(installer), '--help'], capture_output=True).returncode == 0
assert subprocess.run([str(installer), '--unknown'], capture_output=True).returncode == 2
with tempfile.TemporaryDirectory(prefix='ludash-login-test-') as directory:
    folder = Path(directory)
    binary = folder / 'bin'
    binary.mkdir()
    def executable(name, content):
        target = binary / name
        target.write_text(content)
        target.chmod(0o755)
    executable('ludash-session', (root / 'scripts/ludash-session').read_text())
    executable('id', '#!/bin/sh\necho 1000\n')
    for command in ('quickshell', 'konsole', 'fish', 'pacman'):
        executable(command, '#!/bin/sh\nexit 97\n')
    executable('dbus-run-session', '#!/bin/sh\n[ "$1" = -- ] || exit 2\nshift\nexec "$@"\n')
    executable('ludash-compositor', f'#!{sys.executable}\nimport json, os, sys\nfrom pathlib import Path\nPath(os.environ["TEST_RECORD"]).write_text(json.dumps({{"args": sys.argv[1:], "env": dict(os.environ)}}))\n')
    record = folder / 'launch.json'
    env = os.environ | {'PATH': str(binary) + os.pathsep + os.environ['PATH'],
                        'XDG_RUNTIME_DIR': directory, 'XDG_STATE_HOME': str(folder / 'state'),
                        'TEST_RECORD': str(record), 'DISPLAY': ':999', 'WAYLAND_DISPLAY': 'host',
                        'LIBGL_ALWAYS_SOFTWARE': '1', 'MESA_GL_VERSION_OVERRIDE': '2.1',
                        'MESA_GLSL_VERSION_OVERRIDE': '120', 'QT_QUICK_BACKEND': 'software'}
    for key in ('LUDASH_GRAPHICS', 'QT_QPA_EGLFS_INTEGRATION'):
        env.pop(key, None)
    launcher = str(binary / 'ludash-session')
    def run(*args, environment=env):
        return subprocess.run([launcher, *args], env=environment, capture_output=True, text=True)
    assert run('--check').returncode == 0
    assert not record.exists(), 'Preflight launched the compositor'
    assert run(environment=env | {'XDG_RUNTIME_DIR': ''}).returncode != 0
    assert run(environment=env | {'LUDASH_GRAPHICS': 'invalid'}).returncode == 2
    literal = 'socket;echo injected'
    result = run('--socket', literal)
    assert result.returncode == 0, result.stderr
    launch = json.loads(record.read_text())
    assert launch['args'] == ['--fullscreen', '--graphics', 'gles', '--socket', literal]
    assert launch['env']['QT_QPA_PLATFORM'] == 'eglfs'
    assert launch['env']['QT_QPA_EGLFS_INTEGRATION'] == 'eglfs_kms'
    assert launch['env']['XDG_CURRENT_DESKTOP'] == 'LuDash'
    for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'LIBGL_ALWAYS_SOFTWARE', 'MESA_GL_VERSION_OVERRIDE', 'MESA_GLSL_VERSION_OVERRIDE', 'QT_QUICK_BACKEND'):
        assert name not in launch['env'], name
    logs = list((folder / 'state/ludash').glob('session-*.log'))
    assert len(logs) == 1 and logs[0].stat().st_mode & 0o777 == 0o600
    result = subprocess.run([str(installer), '--dry-run'], env=env, capture_output=True, text=True)
    if os.geteuid() == 0:
        assert result.returncode == 1 and 'normal user' in result.stderr
    else:
        assert result.returncode == 0, result.stderr
        assert 'makepkg --syncdeps --force --install' in result.stdout
        assert 'systemctl enable' not in result.stdout
        assert '97' not in result.stdout
        # Emulate a machine without an enabled display manager. No fake tool
        # should execute: all boot configuration remains a dry-run plan.
        executable('readlink', '#!/bin/sh\nexit 0\n')
        executable('systemctl', '#!/bin/sh\nexit 97\n')
        result = subprocess.run([str(installer), '--enable-sddm', '--dry-run'], env=env, capture_output=True, text=True)
        assert result.returncode == 0, result.stderr
        assert 'systemctl enable sddm.service' in result.stdout
        assert 'systemctl set-default graphical.target' in result.stdout
        assert '--now' not in result.stdout
print('Login scripts passed: no side effects in preflight/dry-run, isolated environment, literal arguments and private logs.')
