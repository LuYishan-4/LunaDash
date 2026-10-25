#!/usr/bin/env python3
"""Every compositor command the LunaDash shell sends must have a handler.

The Slint shell talks to the compositor through SlintLu's `Compositor.command(method, value)`.
This review finds every literal call in the pinned shell checkout and fails when the compositor
has no handler for the method, or when `launch-default` names a role the compositor rejects.
"""
from pathlib import Path
import os
import re
import sys

root = Path(__file__).resolve().parents[2]
shell_root = Path(os.environ.get("LUDASH_SHELL_SOURCE_DIR")
                  or os.environ.get("LUDASH_QML_SOURCE_DIR", root / "shell")).resolve()
compositor = (root / "src/compositor/wayland/WaylandCompositor.cpp").read_text(encoding="utf-8")

if not (shell_root / "shell.slint").is_file():
    sys.exit(f"The pinned LunaDash-Shell checkout has no shell.slint: {shell_root}")

supported = set(re.findall(r'method\s*==\s*"([^"]+)"', compositor))
launch_block = compositor[compositor.index('method == "launch-default"'):]
launch_block = launch_block[: launch_block.index("{")]
launch_roles = set(re.findall(r'value\s*==\s*"([^"]+)"', launch_block))

call = re.compile(r'Compositor\.command\(\s*"([^"]+)"\s*,\s*("([^"]*)")?')
methods, roles, problems = set(), set(), []
for path in sorted(shell_root.rglob("*.slint")):
    for match in call.finditer(path.read_text(encoding="utf-8")):
        method, role = match.group(1), match.group(3)
        methods.add(method)
        if method not in supported:
            problems.append(f"{path.relative_to(shell_root)}: no compositor handler for {method!r}")
        if method == "launch-default" and role is not None:
            roles.add(role)
            if role not in launch_roles:
                problems.append(
                    f"{path.relative_to(shell_root)}: launch-default role {role!r} is not one of "
                    + ", ".join(sorted(launch_roles)))

if not methods:
    problems.append("no Compositor.command calls found: the shell is not integrated with LunaDash")
if problems:
    print("\n".join(problems), file=sys.stderr)
    raise SystemExit(1)
print("Shell action review passed")
print("  compositor commands:", ", ".join(sorted(methods)))
print("  default application roles:", ", ".join(sorted(roles)))
