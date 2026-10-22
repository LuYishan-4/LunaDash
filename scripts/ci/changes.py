#!/usr/bin/env python3
"""Select broad CI suites from a complete Git diff; unknown paths fail open."""
import json
import os
from pathlib import Path
import subprocess
import re
import urllib.parse
import urllib.request

SUITES = ("native", "qml", "website", "nix", "distro", "analysis")


def select(paths, full=False):
    selected = dict.fromkeys(SUITES, full)
    if full:
        return selected
    for path in paths:
        if path.startswith((".github/", "scripts/ci/", "tests/ci/", "scripts/security/", "tests/security/")) or path == ".clang-tidy":
            return dict.fromkeys(SUITES, True)
        if path.startswith(("site/", "tests/site/", "scripts/site/", "docs/brand/", "docs/image/")):
            selected["website"] = True
        elif path.startswith(("docs/",)) or path.endswith(".md") or path in ("LICENSE", ".gitignore", ".clang-format"):
            continue
        elif path in ("flake.nix", "flake.lock") or path.startswith("nix/"):
            selected["nix"] = True
        elif path.startswith(("qml/", "tests/qml/")):
            selected.update(qml=True, native=True, nix=True)
        elif path.startswith(("src/", "cmake/", "data/", "protocols/", "templates/", "tests/", "scripts/", "packaging/")) or path in ("CMakeLists.txt", "install.sh"):
            selected.update(native=True, qml=True, nix=True, distro=True)
        else:
            # A newly introduced domain must never silently lose coverage.
            return dict.fromkeys(SUITES, True)
    return selected


def successful_push_base(event):
    """Carry unverified changes across cancelled or failed push runs."""
    if not os.environ.get("GITHUB_ACTIONS"):
        return event.get("before")
    repository = os.environ.get("GITHUB_REPOSITORY", "")
    workflow = os.environ.get("GITHUB_WORKFLOW", "")
    branch = event.get("ref", "").removeprefix("refs/heads/")
    if not repository or not workflow or not branch:
        return None
    query = urllib.parse.urlencode({
        "branch": branch, "event": "push", "status": "success", "per_page": 100,
    })
    request = urllib.request.Request(
        f"https://api.github.com/repos/{repository}/actions/runs?{query}",
        headers={"Accept": "application/vnd.github+json"},
    )
    token = os.environ.get("GH_TOKEN")
    if token:
        request.add_header("Authorization", "Bearer " + token)
    try:
        with urllib.request.urlopen(request, timeout=10) as response:
            runs = json.load(response)["workflow_runs"]
        for run in runs:
            if run.get("name") == workflow and re.fullmatch(r"[0-9a-f]{40}", run.get("head_sha", "")):
                return run["head_sha"]
    except (OSError, ValueError, KeyError):
        pass
    # API outages or a branch without successful CI must expand coverage.
    print("No successful branch CI baseline available; selecting every suite.")
    return None


def changed_paths(event, event_name):
    if event_name == "pull_request":
        base = event["pull_request"]["base"]["sha"]
        head = event["pull_request"]["head"]["sha"]
        revision = f"{base}...{head}"
    elif event_name == "push":
        base = event.get("before", "")
        if not base or set(base) == {"0"}:
            return None
        base = successful_push_base(event)
        if not base:
            return None
        revision = f"{base}..{event['after']}"
    elif event_name == "workflow_dispatch":
        revision = "HEAD^..HEAD"
    else:
        return None
    try:
        diff = subprocess.run(
            ["git", "diff", "--no-renames", "--name-only", "-z", revision, "--"],
            check=True, capture_output=True, text=True,
        )
    except subprocess.CalledProcessError:
        return None
    return list(filter(None, diff.stdout.split("\0")))


def main():
    event = json.loads(Path(os.environ["GITHUB_EVENT_PATH"]).read_text())
    paths = changed_paths(event, os.environ["GITHUB_EVENT_NAME"])
    full = os.environ.get("CI_FULL") == "true" or paths is None
    selected = select(paths or [], full)
    print(json.dumps({"full": full, "paths": paths, "suites": selected}, indent=2))
    with open(os.environ["GITHUB_OUTPUT"], "a") as output:
        for name, enabled in selected.items():
            print(f"{name}={str(enabled).lower()}", file=output)


if __name__ == "__main__":
    main()
