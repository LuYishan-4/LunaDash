#!/usr/bin/env python3
import os
import subprocess
import sys
import tempfile


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
        path == ".gitmodules"
        or (path.startswith("site/src/pages/releases/") and path.lower().endswith((".md", ".mdx")))
        or path == "site/src/data/releases.json"
    )


def component_revision(commit: str, path: str) -> str | None:
    result = subprocess.run(
        ["git", "ls-tree", commit, "--", path],
        check=True,
        text=True,
        capture_output=True,
    )
    fields = result.stdout.split()
    if len(fields) != 4 or fields[0] != "160000":
        return None
    return fields[2]


def changed_site_component_files(base_sha: str, head_sha: str) -> list[str]:
    """Inspect the pinned website commits because the root diff only shows a gitlink."""
    old_revision = component_revision(base_sha, "site")
    new_revision = component_revision(head_sha, "site")
    if not old_revision or not new_revision or old_revision == new_revision:
        return []

    with tempfile.TemporaryDirectory(prefix="lunadash-site-policy-") as temporary:
        repository = temporary + "/site"
        subprocess.run(["git", "init", "-q", repository], check=True)
        subprocess.run(
            ["git", "-C", repository, "remote", "add", "origin",
             "https://github.com/LunaDash-DE/LunaDash-docs.git"],
            check=True,
        )
        subprocess.run(
            ["git", "-C", repository, "fetch", "--quiet", "--no-tags", "--depth=1",
             "origin", old_revision, new_revision],
            check=True,
        )
        result = subprocess.run(
            ["git", "-C", repository, "diff", "--no-renames", "--name-only", "-z",
             old_revision, new_revision, "--"],
            check=True,
            text=True,
            capture_output=True,
        )
        return [path for path in result.stdout.split("\0") if path]


def main() -> int:
    base_ref = os.environ.get("PR_BASE_REF", "")
    base_sha = os.environ.get("PR_BASE_SHA", "")
    head_sha = os.environ.get("PR_HEAD_SHA", "")
    event_name = os.environ.get("GITHUB_EVENT_NAME", "")

    if event_name != "pull_request":
        print("PR policy applies only to pull_request events.")
        return 0

    promotion = (
        base_ref == "main"
        and os.environ.get("PR_HEAD_REF") == "dev"
        and os.environ.get("PR_HEAD_REPO") == os.environ.get("GITHUB_REPOSITORY")
        and bool(os.environ.get("GITHUB_REPOSITORY"))
    )
    errors: list[str] = []
    if base_ref != "dev" and not promotion:
        errors.append(f"Pull requests must target dev, not {base_ref or '<unknown>'}.")

    if not base_sha or not head_sha:
        errors.append("Missing pull-request base or head SHA.")
    else:
        files = changed_files(base_sha, head_sha)
        component_files = (
            [f"site/{path}" for path in changed_site_component_files(base_sha, head_sha)]
            if "site" in files else []
        )
        policy_files = files + component_files
        forbidden = [path for path in policy_files if protected_path(path)]
        for path in ([] if promotion else forbidden):
            errors.append(f"Pull requests must not change generated release notes: {path!r}.")
        print("Changed files:")
        for path in policy_files:
            print(f"  {path!r}")

    if errors:
        print("\nPR policy failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    print("PR policy passed: same-repository dev promotion." if promotion
          else "PR policy passed: target=dev and generated release notes unchanged.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
