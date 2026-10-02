"""A Nix-managed update must report the system workflow before authorization."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class NixUpdateTests(unittest.TestCase):
    def test_update_and_rollback_leave_system_to_nix(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = root / "bin"
            binary.mkdir()
            for name in ("pkexec", "git"):
                tool = binary / name
                tool.write_text('#!/bin/sh\ntouch "$TEST_UNEXPECTED_COMMAND"\nexit 99\n')
                tool.chmod(0o755)
            env = os.environ | {
                "PATH": str(binary) + os.pathsep + os.environ["PATH"],
                "XDG_STATE_HOME": str(root / "state"),
                "LUNADASH_PACKAGE_MANAGER": "nix",
                "TEST_UNEXPECTED_COMMAND": str(root / "unexpected"),
            }
            for args in (("dev", "abcdef"), ("--rollback",)):
                result = subprocess.run(["bash", str(ROOT / "scripts/lunadash-update"), *args],
                                        env=env, capture_output=True, text=True, timeout=10)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("nixos-rebuild", result.stderr)
                self.assertFalse((root / "unexpected").exists())
                status = subprocess.run(["bash", str(ROOT / "scripts/lunadash-update"), "--status"],
                                        env=env, capture_output=True, text=True, check=True, timeout=10)
                self.assertEqual(json.loads(status.stdout)["state"], "error")


if __name__ == "__main__":
    unittest.main()
