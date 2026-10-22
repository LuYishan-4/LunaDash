"""The main promotion exception must never apply to forks or feature branches."""
import importlib.util
import os
from pathlib import Path
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location(
    "policy", Path(__file__).resolve().parents[2] / "scripts/ci/check_pr_policy.py")
policy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(policy)


class PolicyTests(unittest.TestCase):
    def check(self, base, head, repo, files):
        with (
            patch.dict(os.environ, {
                "GITHUB_EVENT_NAME": "pull_request", "PR_BASE_REF": base,
                "PR_HEAD_REF": head, "PR_BASE_SHA": "base", "PR_HEAD_SHA": "head",
                "GITHUB_REPOSITORY": "owner/project", "PR_HEAD_REPO": repo,
            }, clear=True),
            patch.object(policy, "changed_files", return_value=files),
            patch.object(policy, "changed_site_component_files", return_value=[]),
        ):
            return policy.main()

    def check_site_pin(self, component_files):
        with (
            patch.dict(os.environ, {
                "GITHUB_EVENT_NAME": "pull_request", "PR_BASE_REF": "dev",
                "PR_HEAD_REF": "feature", "PR_BASE_SHA": "base", "PR_HEAD_SHA": "head",
                "GITHUB_REPOSITORY": "owner/project", "PR_HEAD_REPO": "fork/project",
            }, clear=True),
            patch.object(policy, "changed_files", return_value=["site"]),
            patch.object(policy, "changed_site_component_files", return_value=component_files),
        ):
            return policy.main()

    def test_same_repository_dev_promotion(self):
        self.assertEqual(self.check("main", "dev", "owner/project", [".github/workflows/dev-ci.yml"]), 0)

    def test_fork_cannot_promote(self):
        self.assertNotEqual(self.check("main", "dev", "fork/project", ["src/File.cpp"]), 0)

    def test_feature_branch_cannot_promote(self):
        self.assertNotEqual(self.check("main", "feature", "owner/project", []), 0)

    def test_workflow_contributions_are_reviewable(self):
        self.assertEqual(self.check("dev", "feature", "owner/project", [".github/workflows/dev-ci.yml"]), 0)

    def test_generated_release_notes_remain_protected(self):
        self.assertNotEqual(self.check("dev", "feature", "fork/project", ["site/src/data/releases.json"]), 0)

    def test_generated_release_markdown_remains_protected(self):
        self.assertNotEqual(
            self.check("dev", "feature", "fork/project", ["site/src/pages/releases/1.0.1a.md"]), 0
        )

    def test_site_code_changes_are_reviewable(self):
        self.assertEqual(
            self.check("dev", "feature", "fork/project", ["site/src/pages/docs/start.astro"]), 0
        )

    def test_site_pin_cannot_change_generated_release_files(self):
        self.assertNotEqual(
            self.check_site_pin(["src/pages/releases/1.0.1a.md"]), 0
        )

    def test_site_pin_allows_ordinary_website_changes(self):
        self.assertEqual(
            self.check_site_pin(["src/pages/docs/start.astro"]), 0
        )

    def test_documentation_contribution_allowed(self):
        self.assertEqual(self.check("dev", "feature", "fork/project", ["docs/en/NIXOS.md"]), 0)


if __name__ == "__main__":
    unittest.main()
