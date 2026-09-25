"""Exercise installer control flow without privileges, builds or package changes."""
import json
import os
from pathlib import Path
import pty
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="lunadash-installer-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.project = self.root / "project with spaces"
        (self.project / "scripts").mkdir(parents=True)
        (self.project / "packaging/arch").mkdir(parents=True)
        shutil.copy2(ROOT / "scripts/install-session.sh", self.project / "scripts/install-session.sh")
        shutil.copy2(ROOT / "scripts/setup-guide.sh", self.project / "scripts/setup-guide.sh")
        (self.project / "scripts/make-source.sh").write_text("#!/bin/bash\nexit 0\n")
        (self.project / "scripts/make-source.sh").chmod(0o755)
        self.bin = self.root / "bin"
        self.bin.mkdir()
        for command in ("bash", "dirname", "mv", "cat", "tr"):
            (self.bin / command).symlink_to(shutil.which(command))
        self.env = dict(os.environ, PATH=str(self.bin), TEST_EVENTS=str(self.root / "events"),
                        LUDASH_INSTALL_PROGRESS_FILE=str(self.root / "progress"),
                        HOME=str(self.root / "home"), XDG_CONFIG_HOME=str(self.root / "config"),
                        XDG_STATE_HOME=str(self.root / "state"))
        for name in ("LUDASH_PREFER_PKEXEC", "LUDASH_UPDATE_MODE", "LUDASH_BUILD_DIR"):
            self.env.pop(name, None)
        agent = self.project / "scripts/lunadash-polkit-agent"
        agent.write_text("#!/bin/bash\nif [[ ${1:-} == --print ]]; then echo available; exit 0; fi\nexec fake-agent\n")
        agent.chmod(0o755)
        self.tool("fake-agent", '''
Path(os.environ["TEST_EVENTS"] + ".agent").write_text(str(os.getpid()))
while True:
    time.sleep(1)
''')
        self.tool("makepkg", '''
if "--packagelist" in args:
    print(Path.cwd() / "ludash.pkg.tar.zst")
elif "--install" not in args:
    assert "--noconfirm" in args
    (Path.cwd() / "ludash.pkg.tar.zst").touch()
''')
        self.tool("pacman", '''
assert args[0] == "-U" and "--noconfirm" in args, "package confirmation would need input"
assert sys.stdin.read() == "", "background package installation must receive EOF"
for attempt in range(100):
    if Path(os.environ["LUDASH_INSTALL_PROGRESS_FILE"]).read_text().startswith("84|install|"):
        break
    time.sleep(0.01)
else:
    raise AssertionError("installation was not reported after authorization")
assert Path(args[-1]).is_file()
sys.exit(int(os.environ.get("TEST_INSTALL_EXIT", "0")))
''')
        self.tool("pkexec", '''
assert Path(os.environ["TEST_EVENTS"] + ".agent").exists(), "authentication agent was never started"
assert args.pop(0) == "--disable-internal-agent", "must not start an invisible terminal agent"
assert Path(os.environ["LUDASH_INSTALL_PROGRESS_FILE"]).read_text().startswith("82|authorization|")
assert sys.stdin.read() == ""
if os.environ.get("TEST_AUTH_DENIED"):
    sys.exit(126)
os.execv(args[0], args)
''')
        self.tool("sudo", '''
assert args.pop(0) == "-n", "non-interactive elevation must not prompt"
os.execvp(args[0], args)
''')

    def tool(self, name, body):
        path = self.bin / name
        path.write_text(f"#!{sys.executable}\n" + '''import json, os, sys, time
from pathlib import Path
args = sys.argv[1:]
with open(os.environ["TEST_EVENTS"], "a") as log:
    log.write(json.dumps([Path(sys.argv[0]).name, *args]) + "\\n")
''' + body)
        path.chmod(0o755)

    def run_installer(self, *options, graphical=True, extra_env=None):
        env = self.env.copy()
        if graphical:
            env["LUDASH_PREFER_PKEXEC"] = "1"
        env.update(extra_env or {})
        return subprocess.run([str(self.bin / "bash"), str(self.project / "scripts/install-session.sh"),
                               "--skip-deps", *options], env=env, input="", text=True,
                              capture_output=True, timeout=10)

    def events(self):
        path = self.root / "events"
        return [json.loads(line) for line in path.read_text().splitlines()] if path.exists() else []

    def test_agent_launcher_uses_wayland_without_portal_theme(self):
        self.tool("polkit-kde-authentication-agent-1", '''
assert os.environ["QT_QPA_PLATFORM"] == "wayland"
assert "QT_QPA_PLATFORMTHEME" not in os.environ
''')
        env = dict(self.env, WAYLAND_DISPLAY="isolated-test-display",
                   QT_QPA_PLATFORM="eglfs", QT_QPA_PLATFORMTHEME="xdgdesktopportal")
        launcher = str(ROOT / "scripts/lunadash-polkit-agent")
        selected = subprocess.run([launcher, "--print"], env=env, text=True,
                                  capture_output=True, timeout=5)
        self.assertEqual(selected.returncode, 0, selected.stderr)
        self.assertEqual(selected.stdout.strip(), str(self.bin / "polkit-kde-authentication-agent-1"))
        result = subprocess.run([launcher], env=env, text=True, capture_output=True, timeout=5)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_graphical_update_confirms_packages_without_input(self):
        result = self.run_installer(extra_env={"LUDASH_UPDATE_MODE": "1"})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue((self.root / "progress").read_text().startswith("96|finalize|"))
        self.assertTrue(any(row[0] == "pacman" for row in self.events()))
        pid = int(Path(str(self.root / "events") + ".agent").read_text())
        with self.assertRaises(ProcessLookupError):
            os.kill(pid, 0)

    def test_authorization_denial_never_runs_package_installation(self):
        result = self.run_installer("--non-interactive", extra_env={"TEST_AUTH_DENIED": "1"})
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(any(row[0] == "pacman" for row in self.events()))
        self.assertTrue((self.root / "progress").read_text().startswith("82|authorization|"))
        self.assertIn("authorization", result.stdout + result.stderr)

    def test_package_failure_is_not_reported_as_success(self):
        result = self.run_installer("--non-interactive", extra_env={"TEST_INSTALL_EXIT": "9"})
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue((self.root / "progress").read_text().startswith("84|install|"))

    def test_noninteractive_terminal_path_uses_separate_install(self):
        result = self.run_installer("--non-interactive", graphical=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(any(row[:2] == ["sudo", "-n"] for row in self.events()))
        self.assertFalse(any("--install" in row for row in self.events()))

    def test_normal_terminal_install_retains_interactive_makepkg(self):
        result = self.run_installer(graphical=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(any(row[0] == "makepkg" and "--install" in row for row in self.events()))

    def test_dry_run_does_not_require_prebuilt_packages_or_elevate(self):
        result = self.run_installer("--non-interactive", "--dry-run")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertFalse(any(row[0] in ("pkexec", "sudo", "pacman") for row in self.events()))
        self.assertFalse((self.project / "packaging/arch/ludash.pkg.tar.zst").exists())

    def test_cmake_install_uses_the_same_authorization_flow(self):
        (self.bin / "pacman").unlink()
        self.tool("cmake", '''
if "--install" in args:
    assert sys.stdin.read() == ""
''')
        self.tool("ninja", "pass\n")
        result = self.run_installer("--non-interactive")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(any(row[:2] == ["cmake", "--install"] for row in self.events()))

    def test_update_cannot_apply_first_install_choices(self):
        for options in (("--guided",), ("--author-config",), ("--desktop-profile",),
                        ("--apps", "basics")):
            result = self.run_installer("--non-interactive", *options)
            self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertEqual(self.events(), [])

    def test_unknown_app_group_fails_before_installation(self):
        for group in ("unknown", "basics,", ",basics", "basics,,desktop", "basics;touch /tmp/no"):
            result = self.run_installer("--skip-guide", "--apps", group, graphical=False)
            self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertEqual(self.events(), [])

    def test_explicit_guide_requires_a_terminal(self):
        result = self.run_installer("--guided", graphical=False)
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn("requires a terminal", result.stderr)
        self.assertEqual(self.events(), [])

    def test_first_install_dry_run_does_not_write_profiles(self):
        result = self.run_installer("--dry-run", "--apps", "basics,input", "--desktop-profile",
                                    "--author-config", graphical=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("fcitx5-chewing", result.stdout)
        self.assertIn("setup-profile.py", result.stdout)
        self.assertFalse((self.root / "config").exists())
        self.assertFalse((self.root / "state").exists())
        self.assertEqual(self.events(), [])

    def test_guide_cancellation_never_builds_or_installs(self):
        master, slave = pty.openpty()
        try:
            with subprocess.Popen([str(self.bin / "bash"), str(self.project / "scripts/install-session.sh"),
                                   "--skip-deps", "--guided"], env=self.env, stdin=slave,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True) as process:
                os.write(master, b"n\n" * 9)
                stdout, stderr = process.communicate(timeout=10)
            self.assertEqual(process.returncode, 0, stdout + stderr)
            self.assertIn("Setup cancelled before installation", stdout)
            self.assertEqual(self.events(), [])
        finally:
            os.close(master)
            os.close(slave)


if __name__ == "__main__":
    unittest.main()
