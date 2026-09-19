#!/usr/bin/env python3
import os
import subprocess
import sys


def changed_files(base_sha: str, head_sha: str) -> list[str]:
    result = subprocess.run(
        ["git", "diff", "--no-renames", "--name-only", "-z", f"{base_sha}...{head_sha}", "--"],
        check=True,
        text=True,
        capture_output=True,
    )
    return [path for path in result.stdout.split("\0") if path]


def protected_path(path: str) -> bool:
    return (
        path.startswith(".github/workflows/")
        or (path.startswith("site/src/pages/releases/") and path.lower().endswith((".md", ".mdx")))
        or path == "site/src/data/releases.json"
    )


def main() -> int:
    base_ref = os.environ.get("PR_BASE_REF", "")
    base_sha = os.environ.get("PR_BASE_SHA", "")
    head_sha = os.environ.get("PR_HEAD_SHA", "")
    event_name = os.environ.get("GITHUB_EVENT_NAME", "")

    if event_name != "pull_request":
        print("PR policy applies only to pull_request events.")
        return 0

    errors: list[str] = []
    if base_ref != "dev":
        errors.append(f"Pull requests must target dev, not {base_ref or '<unknown>'}.")

    if not base_sha or not head_sha:
        errors.append("Missing pull-request base or head SHA.")
    else:
        files = changed_files(base_sha, head_sha)
        forbidden = [path for path in files if protected_path(path)]
        for path in forbidden:
            errors.append(f"Pull requests must not change workflows or generated release notes: {path!r}.")
        has_docs = any(path.startswith("docs/") for path in files)
        has_site = any(path.startswith("site/") and not protected_path(path) for path in files)
        if not has_docs:
            errors.append("Every pull request must update at least one file under docs/.")
        if not has_site:
            errors.append("Every pull request must update at least one file under site/.")
        print("Changed files:")
        for path in files:
            print(f"  {path!r}")

    if errors:
        print("\nPR policy failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    print("PR policy passed: target=dev, docs updated, website updated, protected paths unchanged.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
