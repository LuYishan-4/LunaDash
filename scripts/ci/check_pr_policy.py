#!/usr/bin/env python3
import os
import subprocess
import sys


def changed_files(base_sha: str) -> list[str]:
    result = subprocess.run(
        ["git", "diff", "--name-only", f"{base_sha}...HEAD"],
        check=True,
        text=True,
        capture_output=True,
    )
    return [line.strip() for line in result.stdout.splitlines() if line.strip()]


def main() -> int:
    base_ref = os.environ.get("PR_BASE_REF", "")
    base_sha = os.environ.get("PR_BASE_SHA", "")
    event_name = os.environ.get("GITHUB_EVENT_NAME", "")

    if event_name != "pull_request":
        print("PR policy applies only to pull_request events.")
        return 0

    errors: list[str] = []
    if base_ref != "dev":
        errors.append(f"Pull requests must target dev, not {base_ref or '<unknown>'}.")

    if not base_sha:
        errors.append("Missing pull-request base SHA.")
    else:
        files = changed_files(base_sha)
        has_docs = any(path.startswith("docs/") for path in files)
        has_site = any(path.startswith("site/") for path in files)
        if not has_docs:
            errors.append("Every pull request must update at least one file under docs/.")
        if not has_site:
            errors.append("Every pull request must update at least one file under site/.")
        print("Changed files:")
        for path in files:
            print(f"  {path}")

    if errors:
        print("\nPR policy failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    print("PR policy passed: target=dev, docs updated, website updated.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
