"""First-install file preservation and user-editable template regressions."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("setup_profile", ROOT / "scripts/setup-profile.py")
profile = importlib.util.module_from_spec(spec)
spec.loader.exec_module(profile)


class SetupProfileTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="lunadash setup ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.config = self.root / "configuration with spaces"
        self.state = self.root / "state"

    def seed(self):
        profile.seed(self.config, self.state, desktop=True, author=True, complete=True, apps="input")

    def test_new_profile_uses_existing_module_schema_without_custom_code(self):
        self.seed()
        document = json.loads((self.config / "LuDash/shell-modules.json").read_text())
        self.assertEqual(document["schemaVersion"], 1)
        self.assertEqual(set(document["modules"]), {"panel"})
        panel = document["modules"]["panel"]["config"]
        registry = json.loads((ROOT / "data/modules/registry.json").read_text())
        descriptor = next(module for module in registry["modules"] if module["id"] == "panel")
        for name, value in panel.items():
            self.assertIn(name, descriptor["sections"]["config"])
            self.assertIs(value, True)
        self.assertTrue((self.state / "lunadash/setup/complete.json").is_file())

    def test_existing_app_group_and_profile_are_preserved(self):
        (self.config / "kitty").mkdir(parents=True)
        (self.config / "kitty/kitty.conf").write_text("font_size 18\n")
        (self.config / "LuDash").mkdir()
        (self.config / "LuDash/shell-modules.json").write_text("personal profile\n")
        self.seed()
        self.assertEqual((self.config / "kitty/kitty.conf").read_text(), "font_size 18\n")
        self.assertFalse((self.config / "kitty/lunadash.conf").exists())
        self.assertEqual((self.config / "LuDash/shell-modules.json").read_text(), "personal profile\n")
        self.assertTrue((self.config / "LuDash/setup-examples/kitty/kitty.conf").is_file())

    def test_repeated_setup_preserves_user_overrides_and_original_journal(self):
        self.seed()
        custom = self.config / "kitty/__custom__.conf"
        custom.write_text("font_size 17\n")
        marker = self.state / "lunadash/setup/complete.json"
        previous = marker.read_bytes()
        self.seed()
        self.assertEqual(custom.read_text(), "font_size 17\n")
        self.assertEqual(marker.read_bytes(), previous)

    def test_symlink_parent_rejected_before_any_file_is_created(self):
        self.config.mkdir()
        outside = self.root / "outside"
        outside.mkdir()
        (self.config / "kitty").symlink_to(outside, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "symbolic link"):
            self.seed()
        self.assertEqual(list(outside.iterdir()), [])
        self.assertFalse((self.config / "LuDash").exists())
        self.assertFalse(self.state.exists())

    def test_dangling_config_symlink_is_preserved(self):
        (self.config / "kitty").mkdir(parents=True)
        link = self.config / "kitty/kitty.conf"
        link.symlink_to(self.root / "missing")
        self.seed()
        self.assertTrue(link.is_symlink())
        self.assertFalse(link.exists())
        self.assertFalse((self.config / "kitty/lunadash.conf").exists())

    def test_personal_file_permissions_and_custom_override_order(self):
        self.seed()
        kitty = self.config / "kitty/kitty.conf"
        self.assertEqual(kitty.stat().st_mode & 0o777, 0o600)
        self.assertTrue(kitty.read_text().rstrip().endswith("include __custom__.conf"))
        sources = "\n".join(path.read_text() for path in (ROOT / "data/setup/author").rglob("*") if path.is_file())
        self.assertNotIn("/home/luyishan", sources)
        self.assertNotIn("noctalia.conf", sources)
        self.assertNotIn("nyxniri", sources.lower())


if __name__ == "__main__":
    unittest.main()
