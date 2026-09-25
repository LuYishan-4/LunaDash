#!/usr/bin/env python3
"""Seed portable first-install settings without replacing personal files."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys

ASSETS = Path(__file__).resolve().parents[1] / "data/setup"
AUTHOR_GROUPS = {
    "kitty": ("kitty/kitty.conf", "kitty/lunadash.conf", "kitty/__custom__.conf"),
    "fish": ("fish/config.fish", "fish/__custom__.fish"),
    "starship": ("starship.toml",),
    "fastfetch": ("fastfetch/config.jsonc",),
}


def plain_path(path):
    """Reject symlink parents, including dangling links, before writing."""
    for parent in reversed((path, *path.parents)):
        if parent.is_symlink():
            raise ValueError(f"Setup path is a symbolic link; preserve it and merge templates manually: {parent}")
        if parent != path and parent.exists() and not parent.is_dir():
            raise ValueError(f"Setup parent is not a directory: {parent}")


def xdg_path(name, fallback):
    path = Path(os.environ.get(name) or fallback).expanduser()
    if not path.is_absolute():
        raise ValueError(f"{name} must be an absolute path")
    return path


def create_missing(path, content, created):
    # A personal file/directory/link always wins over a setup template.
    if path.exists() or path.is_symlink():
        print(f"Preserved: {path}")
        return
    plain_path(path)
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    try:
        descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    except FileExistsError:
        print(f"Preserved: {path}")
        return
    with os.fdopen(descriptor, "wb") as output:
        output.write(content)
    created.append({"path": str(path), "sha256": hashlib.sha256(content).hexdigest()})
    print(f"Created: {path}")


def seed(config, state, desktop=False, author=False, complete=False, apps="none"):
    planned = []
    examples = config / "LuDash/setup-examples"
    if desktop:
        content = (ASSETS / "desktop-profile.json").read_bytes()
        planned.extend(((examples / "shell-modules.json", content),
                        (config / "LuDash/shell-modules.json", content)))
    if author:
        for group, names in AUTHOR_GROUPS.items():
            # Avoid partially mixing a starter template into an existing app setup.
            destinations = [config / name for name in names]
            occupied = any(path.exists() or path.is_symlink() for path in destinations)
            if occupied:
                print(f"Preserved existing {group} settings; starter files are in {examples}")
            for name, destination in zip(names, destinations):
                content = (ASSETS / "author" / name).read_bytes()
                planned.append((examples / name, content))
                if not occupied:
                    planned.append((destination, content))
    marker = state / "lunadash/setup/complete.json"
    # Validate every new destination before creating anything.
    for path, _ in planned:
        if not path.exists() and not path.is_symlink():
            plain_path(path)
    if complete and not marker.exists() and not marker.is_symlink():
        plain_path(marker)
    created = []
    for path, content in planned:
        create_missing(path, content, created)
    if complete:
        content = json.dumps({"version": "1.0.1a", "apps": apps,
                              "desktopProfile": desktop, "authorConfig": author,
                              "createdFiles": created}, indent=2).encode() + b"\n"
        create_missing(marker, content, [])
    if desktop or author:
        print(f"Editable starter examples: {examples}")
        print("Existing settings were retained. Back them up before manually merging an example.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--desktop-profile", action="store_true")
    parser.add_argument("--author-config", action="store_true")
    parser.add_argument("--complete", action="store_true")
    parser.add_argument("--apps", default="none")
    args = parser.parse_args()
    if os.geteuid() == 0:
        parser.error("Run setup as your normal user.")
    config = xdg_path("XDG_CONFIG_HOME", str(Path.home() / ".config"))
    state = xdg_path("XDG_STATE_HOME", str(Path.home() / ".local/state"))
    seed(config, state, args.desktop_profile, args.author_config, args.complete, args.apps)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        print(f"Setup failed: {error}", file=sys.stderr)
        sys.exit(1)
