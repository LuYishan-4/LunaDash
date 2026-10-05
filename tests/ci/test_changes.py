"""Protect CI selection against missing new domains, deletions and promotions."""
import importlib.util
from pathlib import Path
import unittest
from unittest.mock import patch
import io
import json

spec = importlib.util.spec_from_file_location(
    "changes", Path(__file__).resolve().parents[2] / "scripts/ci/changes.py")
changes = importlib.util.module_from_spec(spec)
spec.loader.exec_module(changes)


class SelectionTests(unittest.TestCase):
    def test_docs_do_not_rebuild_desktop(self):
        self.assertFalse(any(changes.select(["docs/en/NIXOS.md", "README.md"]).values()))

    def test_website_has_independent_suite(self):
        self.assertEqual([name for name, on in changes.select(["site/src/pages/index.astro"]).items() if on], ["website"])

    def test_native_changes_cover_all_native_domains(self):
        for path in ["src/compositor/input/Input.cpp", "src/service/portal/Portal.cpp", "data/translations/en_US.json", "cmake/LunaDashMain.cmake"]:
            selected = changes.select([path])
            self.assertTrue(all(selected[name] for name in ("native", "qml", "nix", "distro")))
            self.assertFalse(selected["analysis"])

    def test_nix_change_does_not_rebuild_other_distributions(self):
        self.assertEqual([name for name, on in changes.select(["flake.lock", "nix/module.nix"]).items() if on], ["nix"])

    def test_unknown_and_ci_paths_get_full_coverage(self):
        for path in ["new-domain/input.conf", ".github/workflows/ci.yml", "tests/ci/test_changes.py"]:
            self.assertTrue(all(changes.select([path]).values()))

    def test_main_full_includes_documentation_only_changes(self):
        self.assertTrue(all(changes.select(["README.md"], full=True).values()))

    @patch.dict(changes.os.environ, {}, clear=True)
    @patch.object(changes.subprocess, "run")
    def test_push_diff_keeps_removed_and_renamed_paths(self, run):
        run.return_value.stdout = "src/Old.cpp\0docs/new.md\0"
        self.assertEqual(changes.changed_paths({"before": "abc", "after": "def"}, "push"), ["src/Old.cpp", "docs/new.md"])
        args = run.call_args.args[0]
        self.assertIn("--no-renames", args)
        self.assertIn("abc..def", args)

    @patch.object(changes, "successful_push_base", return_value="last-green")
    @patch.object(changes.subprocess, "run")
    def test_push_includes_changes_from_cancelled_previous_run(self, run, baseline):
        run.return_value.stdout = ".github/workflows/dev-ci.yml\0data/translations/en_US.json\0"
        paths = changes.changed_paths({"before": "cancelled", "after": "current"}, "push")
        self.assertIn("last-green..current", run.call_args.args[0])
        self.assertTrue(all(changes.select(paths).values()))

    @patch.dict(changes.os.environ, {"GITHUB_ACTIONS": "true", "GITHUB_REPOSITORY": "owner/repo", "GITHUB_WORKFLOW": "Dev CI"}, clear=True)
    @patch.object(changes.urllib.request, "urlopen")
    def test_baseline_uses_successful_matching_workflow(self, fetch):
        sha = "a" * 40
        fetch.return_value = io.BytesIO(json.dumps({"workflow_runs": [
            {"name": "Other workflow", "head_sha": "b" * 40},
            {"name": "Dev CI", "head_sha": sha},
        ]}).encode())
        self.assertEqual(changes.successful_push_base({"ref": "refs/heads/dev"}), sha)
        self.assertIn("branch=dev&event=push&status=success", fetch.call_args.args[0].full_url)

    @patch.dict(changes.os.environ, {"GITHUB_ACTIONS": "true", "GITHUB_REPOSITORY": "owner/repo", "GITHUB_WORKFLOW": "Dev CI"}, clear=True)
    @patch.object(changes.urllib.request, "urlopen", side_effect=OSError("offline"))
    def test_unavailable_baseline_requires_full_coverage(self, fetch):
        self.assertIsNone(changes.changed_paths({"before": "old", "after": "new", "ref": "refs/heads/dev"}, "push"))

    def test_new_branch_and_merge_queue_run_everything(self):
        self.assertIsNone(changes.changed_paths({"before": "0" * 40}, "push"))
        self.assertIsNone(changes.changed_paths({}, "merge_group"))


if __name__ == "__main__":
    unittest.main()
