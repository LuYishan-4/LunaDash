"""Exercise the real curl-pipe entry using a controlling terminal, without installs."""
import errno
import fcntl
import os
from pathlib import Path
import pty
import select
import shutil
import subprocess
import tempfile
import termios
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / 'install.sh'


class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='lunadash-installer-test-')
        self.addCleanup(self.temp.cleanup)
        self.home = Path(self.temp.name)
        self.tools = self.home / 'tools'
        self.tools.mkdir()
        self.stub('pacman', '#!/bin/sh\n[ "$1" = -Qq ] && [ "$2" = discord ]\n')
        self.stub('flatpak', '#!/bin/sh\nexit 1\n')
        self.stub('systemctl', '#!/bin/sh\nexit 1\n')
        self.env = os.environ | {
            'HOME': str(self.home), 'XDG_CONFIG_HOME': str(self.home / 'config'),
            'XDG_STATE_HOME': str(self.home / 'state'),
            'PATH': str(self.tools) + os.pathsep + os.environ['PATH'],
            'LANG': 'en_US.UTF-8', 'NO_COLOR': '1', 'TERM': 'dumb',
        }
        # These commands must never run in --dry-run, including reboot/TTY side effects.
        for command in ('sudo', 'su', 'curl', 'git', 'makepkg', 'fc-cache', 'lunadash-shell-tool'):
            self.stub(command, '#!/bin/sh\nprintf "MUTATING_COMMAND_EXECUTED\\n" >&2\nexit 97\n')

    def stub(self, name, content):
        path = self.tools / name
        path.write_text(content)
        path.chmod(0o755)

    def pipeline(self, answers, *args):
        master, slave = pty.openpty()
        attributes = termios.tcgetattr(slave)
        attributes[3] &= ~termios.ECHO
        termios.tcsetattr(slave, termios.TCSANOW, attributes)

        def terminal():
            os.setsid()
            fcntl.ioctl(slave, termios.TIOCSCTTY, 0)

        process = subprocess.Popen(
            ['bash', '-s', '--', *args], stdin=subprocess.PIPE, stdout=slave,
            stderr=slave, env=self.env, pass_fds=(slave,), preexec_fn=terminal,
        )
        os.close(slave)
        try:
            # stdin really contains shell source, as it does in curl ... | bash.
            process.stdin.write(SCRIPT.read_bytes())
            process.stdin.close()
            os.write(master, ('\n'.join(answers) + '\n').encode())
            output = bytearray()
            deadline = time.monotonic() + 20
            while time.monotonic() < deadline:
                readable, _, _ = select.select([master], [], [], 0.1)
                if readable:
                    try:
                        chunk = os.read(master, 65536)
                    except OSError as error:
                        if error.errno != errno.EIO:
                            raise
                        break
                    if not chunk:
                        break
                    output.extend(chunk)
                elif process.poll() is not None:
                    break
            if process.poll() is None:
                process.wait(timeout=3)
            text = output.decode(errors='replace')
            self.assertNotIn('MUTATING_COMMAND_EXECUTED', text)
            return process.returncode, text
        finally:
            if process.poll() is None:
                process.kill()
            process.wait(timeout=3)
            os.close(master)

    def answers(self, language='2', selection=None, locale='0'):
        answers = [language, 'y']
        if not shutil.which('yay', path=self.env['PATH']):
            answers.append('y')
        if selection is None:
            answers.append('n')
        else:
            answers += ['y', selection, 'done', 'y']
        answers.append(locale)
        if locale != '0':
            answers.append('y')
        if not Path('/etc/systemd/system/display-manager.service').exists():
            answers.append('n')
        answers += ['n', 'n']  # no NetworkManager change, no reboot
        return answers

    def test_pipe_uses_tty_and_selection_exclusions(self):
        code, output = self.pipeline(self.answers(selection='all -3 -5'), '--dry-run')
        self.assertEqual(code, 0, output)
        self.assertIn('LunaDash', output)
        self.assertIn('DRY RUN', output)
        self.assertIn('[installed]', output)
        # Package choices change the install plan, not just the visual ticks.
        official = next(line for line in output.splitlines() if 'Repository packages:' in line)
        self.assertNotIn('zed', official)
        self.assertNotIn('obs-studio', official)
        self.assertNotIn('discord', official)
        self.assertIn('kitty', official)
        self.assertIn('visual-studio-code-bin', output)
        self.assertIn('Preview complete; nothing was installed.', output)
        self.assertNotIn('+ sudo -- systemctl reboot', output)
        self.assertFalse((self.home / 'state').exists())
        self.assertFalse((self.home / 'config').exists())

    def test_traditional_chinese_is_independent_of_system_locale(self):
        code, output = self.pipeline(self.answers(language='1', locale='6'), '--dry-run')
        self.assertEqual(code, 0, output)
        self.assertIn('步驟 1/5', output)
        self.assertIn('步驟 5/5', output)
        self.assertIn('de_DE.UTF-8', output)
        self.assertIn('未重新開機', output)
        self.assertNotIn('LANG=zh_TW.UTF-8', output)
        self.assertNotIn('Step 1/5', output)

    def test_invalid_selection_is_not_evaluated(self):
        answers = self.answers(selection='all -14')
        index = answers.index('all -14')
        answers.insert(index, '$(touch should-not-exist)')
        code, output = self.pipeline(answers, '--dry-run')
        self.assertEqual(code, 0, output)
        self.assertIn('Invalid selection; no choices changed.', output)
        self.assertFalse((self.home / 'should-not-exist').exists())

    def test_declining_base_stops_before_optional_apps(self):
        code, output = self.pipeline(['2', 'n'], '--dry-run')
        self.assertEqual(code, 0, output)
        self.assertNotIn('Step 2/5', output)
        self.assertNotIn('Reboot now?', output)

    def test_noninteractive_invocation_fails_without_installing(self):
        result = subprocess.run(['bash', str(SCRIPT), '--dry-run'], env=self.env,
                                stdin=subprocess.DEVNULL, capture_output=True,
                                text=True, start_new_session=True, timeout=5)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('terminal is required', result.stderr)

    def test_help_and_invalid_arguments_need_no_terminal(self):
        for arguments, expected in [(['--help'], 0), (['--unknown'], 2),
                                     (['--ref', 'dev;touch nope'], 2),
                                     (['--language', 'invalid'], 2)]:
            result = subprocess.run(['bash', str(SCRIPT), *arguments], env=self.env,
                                    stdin=subprocess.DEVNULL, capture_output=True,
                                    timeout=5, start_new_session=True)
            self.assertEqual(result.returncode, expected, result.stderr)

    @unittest.skipUnless(os.geteuid() == 0, 'Root refusal is tested on root container runners')
    def test_root_is_refused_before_package_changes(self):
        code, output = self.pipeline(['2'])
        self.assertNotEqual(code, 0, output)
        self.assertIn('normal user', output)
        self.assertNotIn('+ sudo', output)
        self.assertNotIn('Reboot now?', output)


if __name__ == '__main__':
    unittest.main()
