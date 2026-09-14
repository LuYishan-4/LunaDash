"""Reject documentation-only changes from pull requests."""
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
DOCUMENTATION_PREFIXES = ("docs/", "site/")
DOCUMENTATION_NAMES = {
    "AGENTS.md",
    "CHANGELOG.md",
    "CONTRIBUTING.md",
    "LICENSE",
    "README",
    "README.md",
}
DOCUMENTATION_SUFFIXES = (".md", ".mdx", ".rst", ".adoc", ".txt")


def changed_files(base_revision):
    result = subprocess.run(
        ["git", "-C", str(ROOT), "diff", "--name-only", "--diff-filter=ACMRT", f"{base_revision}...HEAD"],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    )
    return [Path(line) for line in result.stdout.splitlines() if line]


def is_documentation(path):
    normalized = path.as_posix()
    return (
        normalized.startswith(DOCUMENTATION_PREFIXES)
        or path.name in DOCUMENTATION_NAMES
        or path.suffix.lower() in DOCUMENTATION_SUFFIXES
    )


base_revision = os.environ.get("GITHUB_BASE_SHA")
if not base_revision:
    raise SystemExit("GITHUB_BASE_SHA is required for pull-request scope checks.")

documentation = [path for path in changed_files(base_revision) if is_documentation(path)]
if documentation:
    print("Pull requests must not modify documentation files:")
    print("\n".join(f" - {path}" for path in documentation))
    sys.exit(1)

print("Pull-request documentation scope check passed.")
