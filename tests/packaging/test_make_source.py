#!/usr/bin/env python3
"""Run the actual source packager without dependencies, builds or privileges."""
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
PREFIX = "ludash-1.0.1a/"


class SourceArchiveTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="lunadash-source-test-")
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.project = self.work / "project with spaces"
        self.project.mkdir()
        # Copy real working-tree files, not a hand-built fixture that might
        # accidentally recreate a removed directory such as examples/.
        paths = subprocess.run(
            ["git", "-C", str(ROOT), "ls-files", "-z"],
            check=True, capture_output=True, timeout=10,
        ).stdout.split(b"\0")
        for path in filter(None, paths):
            relative = Path(os.fsdecode(path))
            source = ROOT / relative
            if not source.exists() and not source.is_symlink():
                continue  # Respect tracked deletions in a local working tree.
            destination = self.project / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination, follow_symlinks=False)
        self.git("init", "-q")
        self.git("add", "-A")
        self.git("-c", "user.name=Packaging Test",
                 "-c", "user.email=packaging-test@example.invalid",
                 "-c", "commit.gpgsign=false", "commit", "-qm", "Test input")
        self.revision = self.git("rev-parse", "HEAD").stdout.strip()
        self.archive = self.project / "packaging/arch/ludash-1.0.1a.tar.gz"

    def git(self, *args):
        return subprocess.run(
            ["git", "-C", str(self.project), "-c", "core.hooksPath=/dev/null", *args],
            check=True, capture_output=True, text=True, timeout=30,
        )

    def package(self, env=None):
        # Run outside the checkout and with spaces in its path.
        return subprocess.run(
            ["sh", str(self.project / "scripts/make-source.sh")],
            cwd=self.work, env=env, capture_output=True, text=True, timeout=30,
        )

    def assert_packaged(self):
        result = self.package()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(self.archive.is_file())
        self.assert_no_staging_files()

    def assert_no_staging_files(self):
        self.assertEqual(list(self.archive.parent.glob(".make-source.*")), [])

    def test_current_tree_packages_plugins_sdk_and_registry_without_examples(self):
        self.assertFalse((self.project / "examples").exists())
        cache = self.project / "scripts/__pycache__"
        cache.mkdir()
        (cache / "ignored.pyc").write_bytes(b"cache")
        (self.project / "scripts/ignored.pyc").write_bytes(b"cache")
        self.assert_packaged()
        with tarfile.open(self.archive, "r:gz") as archive:
            names = archive.getnames()
            for name in (
                "CMakeLists.txt", "cmake/plugins/LunaDashPlugin.cmake",
                "cmake/plugins/SettingsSchema.py", "src/core/plugins/PluginApi.h",
                "data/plugins/targets.json", "data/plugins/catalog.json",
                "data/modules/registry.json", "templates/plugins/effect-c/Effect.c",
                "templates/plugins/effect-cpp/Effect.cpp",
                "templates/plugins/quickshell/Main.qml",
                "templates/plugins/opengl/Effect.frag",
                "qml/settings/components/SchemaOptions.qml",
                "protocols/wlr-layer-shell-unstable-v1.xml",
                "tests/settings/contract.json", "scripts/install-session.sh",
                "docs/en/SETTINGS_API.md", "docs/zh/SETTINGS_API.md",
                "packaging/arch/PKGBUILD", ".lunadash-revision",
            ):
                self.assertIn(PREFIX + name, names)
            self.assertTrue(all(name.startswith(PREFIX) for name in names))
            self.assertFalse(any("__pycache__" in name or name.endswith(".pyc")
                                 for name in names))
            self.assertNotIn(PREFIX + ".git", names)
            self.assertFalse(any(
                name.startswith(PREFIX + "data/plugins/fade/") or
                name.startswith(PREFIX + "data/plugins/stacking-windows/") or
                name.startswith(PREFIX + "qml/plugins/digital-clock/")
                for name in names
            ))
            self.assertEqual(
                archive.extractfile(PREFIX + ".lunadash-revision").read().decode().strip(),
                self.revision,
            )
            self.assertTrue(archive.getmember(PREFIX + "scripts/install-session.sh").mode & 0o111)
        # Generating an archive must not make the next package appear dirty.
        self.assert_packaged()
        with tarfile.open(self.archive, "r:gz") as archive:
            self.assertEqual(
                archive.extractfile(PREFIX + ".lunadash-revision").read().decode().strip(),
                self.revision,
            )

    def test_local_edits_are_included_and_mark_revision_dirty(self):
        readme = self.project / "README.md"
        readme.write_text(readme.read_text(encoding="utf-8") + "\nLocal packaging edit.\n",
                          encoding="utf-8")
        self.assert_packaged()
        with tarfile.open(self.archive, "r:gz") as archive:
            self.assertEqual(archive.extractfile(PREFIX + "README.md").read(), readme.read_bytes())
            self.assertEqual(
                archive.extractfile(PREFIX + ".lunadash-revision").read().decode().strip(),
                self.revision + "-dirty",
            )

    def test_missing_required_input_preserves_previous_archive(self):
        self.assert_packaged()
        previous = self.archive.read_bytes()
        shutil.rmtree(self.project / "qml")
        result = self.package()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Missing required source path: qml", result.stderr)
        self.assertEqual(self.archive.read_bytes(), previous)
        self.assert_no_staging_files()

    def failing_tar_environment(self):
        tools = self.work / "tools"
        tools.mkdir()
        tar = tools / "tar"
        tar.write_text('#!/bin/sh\nprintf "partial archive" > "$2"\nexit 7\n')
        tar.chmod(0o755)
        return dict(os.environ, PATH=str(tools) + os.pathsep + os.environ["PATH"])

    def test_tar_failure_preserves_previous_archive_and_cleans_staging(self):
        self.assert_packaged()
        previous = self.archive.read_bytes()
        result = self.package(self.failing_tar_environment())
        self.assertEqual(result.returncode, 7, result.stdout + result.stderr)
        self.assertEqual(self.archive.read_bytes(), previous)
        self.assert_no_staging_files()

    def test_tar_failure_does_not_publish_a_partial_first_archive(self):
        result = self.package(self.failing_tar_environment())
        self.assertEqual(result.returncode, 7, result.stdout + result.stderr)
        self.assertFalse(self.archive.exists())
        self.assert_no_staging_files()


if __name__ == "__main__":
    unittest.main()
