"""Exercise installer control flow without privileges, builds or package changes."""
import json
import os
from pathlib import Path
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
        (self.project / "scripts/make-source.sh").write_text("#!/bin/bash\nexit 0\n")
        (self.project / "scripts/make-source.sh").chmod(0o755)
        self.bin = self.root / "bin"
        self.bin.mkdir()
        for command in ("bash", "dirname", "mv", "cat"):
            (self.bin / command).symlink_to(shutil.which(command))
        self.env = dict(os.environ, PATH=str(self.bin), TEST_EVENTS=str(self.root / "events"),
                        LUDASH_INSTALL_PROGRESS_FILE=str(self.root / "progress"))
        for name in ("LUDASH_PREFER_PKEXEC", "LUDASH_UPDATE_MODE", "LUDASH_BUILD_DIR"):
            self.env.pop(name, None)
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

    def test_graphical_update_confirms_packages_without_input(self):
        result = self.run_installer(extra_env={"LUDASH_UPDATE_MODE": "1"})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue((self.root / "progress").read_text().startswith("96|finalize|"))
        self.assertTrue(any(row[0] == "pacman" for row in self.events()))

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


if __name__ == "__main__":
    unittest.main()
