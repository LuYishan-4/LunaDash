#!/usr/bin/env python3
"""Seed portable first-install settings without replacing personal files."""

import argparse
import hashlib
import json
import os
import re
import shlex
import sys
from pathlib import Path

ASSETS = Path(__file__).resolve().parents[1] / "data/setup"

def get_ludash_version():
    """Read LUDASH_RELEASE_VERSION from cmake/LunaDashMain.cmake."""
    cmake_file = Path(__file__).resolve().parents[1] / "cmake" / "LunaDashMain.cmake"
    try:
        content = cmake_file.read_text()
        match = re.search(r'set\(LUDASH_RELEASE_VERSION\s+"([^"]+)"\)', content)
        if match:
            return match.group(1)
    except Exception:
        pass
    return "unknown"
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
            raise ValueError(
                f"Setup path is a symbolic link; preserve it and merge templates manually: {parent}"
            )
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


SHELLS = ("lunadash", "custom", "none")


def write_session(config, shell, command=""):
    """Record the desktop shell choice in LunaDash/session.json.

    This is an explicit choice, so it replaces "shell" (and "shellCommand" for a custom shell)
    while every other key the user wrote, such as "autostart", is kept.
    """
    if shell not in SHELLS:
        raise ValueError(f"The shell must be one of: {', '.join(SHELLS)}")
    arguments = shlex.split(command) if command else []
    if shell == "custom" and not arguments:
        raise ValueError("A custom shell needs a command, for example --shell-command waybar")
    path = config / "LunaDash/session.json"
    plain_path(path)
    document = {}
    if path.exists():
        document = json.loads(path.read_text())
        if not isinstance(document, dict):
            raise ValueError(f"{path} does not contain a JSON object; fix or remove it first")
    document["shell"] = shell
    if arguments:
        document["shellCommand"] = arguments
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(document, indent=2) + "\n")
    os.replace(temporary, path)
    print(f"Desktop shell: {shell}{' (' + ' '.join(arguments) + ')' if arguments else ''} in {path}")


def seed(config, state, desktop=False, author=False, complete=False, apps="none"):
    planned = []
    examples = config / "LuDash/setup-examples"
    if desktop:
        content = (ASSETS / "desktop-profile.json").read_bytes()
        planned.extend(
            (
                (examples / "shell-modules.json", content),
                (config / "LuDash/shell-modules.json", content),
            )
        )
    if author:
        for group, names in AUTHOR_GROUPS.items():
            # Avoid partially mixing a starter template into an existing app setup.
            destinations = [config / name for name in names]
            occupied = any(path.exists() or path.is_symlink() for path in destinations)
            if occupied:
                print(
                    f"Preserved existing {group} settings; starter files are in {examples}"
                )
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
        content = (
            json.dumps(
                {
                    "version": get_ludash_version(),
                    "apps": apps,
                    "desktopProfile": desktop,
                    "authorConfig": author,
                    "createdFiles": created,
                },
                indent=2,
            ).encode()
            + b"\n"
        )
        create_missing(marker, content, [])
    if desktop or author:
        print(f"Editable starter examples: {examples}")
        print(
            "Existing settings were retained. Back them up before manually merging an example."
        )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--desktop-profile", action="store_true")
    parser.add_argument("--author-config", action="store_true")
    parser.add_argument("--complete", action="store_true")
    parser.add_argument("--apps", default="none")
    parser.add_argument("--shell", choices=SHELLS,
                        help="desktop shell for LunaDash sessions (written to session.json)")
    parser.add_argument("--shell-command", default="",
                        help="the custom shell's command line, for example 'waybar -c ~/bar'")
    args = parser.parse_args()
    if args.shell_command and args.shell != "custom":
        parser.error("--shell-command needs --shell custom")
    if os.geteuid() == 0:
        parser.error("Run setup as your normal user.")
    config = xdg_path("XDG_CONFIG_HOME", str(Path.home() / ".config"))
    state = xdg_path("XDG_STATE_HOME", str(Path.home() / ".local/state"))
    if args.shell:
        write_session(config, args.shell, args.shell_command)
    seed(
        config,
        state,
        args.desktop_profile,
        args.author_config,
        args.complete,
        args.apps,
    )


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        print(f"Setup failed: {error}", file=sys.stderr)
        sys.exit(1)
