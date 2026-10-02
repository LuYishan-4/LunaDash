#!/usr/bin/env python3
"""Select broad CI suites from a complete Git diff; unknown paths fail open."""
import json
import os
from pathlib import Path
import subprocess

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


def changed_paths(event, event_name):
    if event_name == "pull_request":
        base = event["pull_request"]["base"]["sha"]
        head = event["pull_request"]["head"]["sha"]
        revision = f"{base}...{head}"
    elif event_name == "push":
        base = event.get("before", "")
        if not base or set(base) == {"0"}:
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
