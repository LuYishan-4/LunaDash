#!/usr/bin/env python3
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[2]
qml_root = root / "qml"
compositor = (root / "src/compositor/WaylandCompositor/WaylandCompositor.cpp").read_text(encoding="utf-8")
desktop = (root / "src/desktop/desktop_main.cpp").read_text(encoding="utf-8")
shell = (qml_root / "shell.qml").read_text(encoding="utf-8")
cmake_text = "\n".join(
    path.read_text(encoding="utf-8")
    for path in (root / "cmake").glob("*.cmake")
)

supported_methods = set(re.findall(r'method\s*==\s*"([^"]+)"', compositor))
qml_methods = set()
qml_launches = set()
direct_url_calls = []

for path in qml_root.rglob("*.qml"):
    text = path.read_text(encoding="utf-8")
    qml_methods.update(
        re.findall(r'\b(?:[A-Za-z_]\w*\.)*command\(\s*"([^"]+)"', text)
    )
    qml_launches.update(
        re.findall(r'\b(?:[A-Za-z_]\w*\.)*launch\(\s*"([^"]+)"', text)
    )
    if "Qt.openUrlExternally" in text:
        direct_url_calls.append(str(path.relative_to(root)))

missing_methods = sorted(qml_methods - supported_methods)
if missing_methods:
    print(
        "QML commands without compositor handlers: " + ", ".join(missing_methods),
        file=sys.stderr,
    )

builtin_ids = set(re.findall(r'id\s*==\s*"([^"]+)"', desktop))
builtin_ids.update(re.findall(r'requested\s*==\s*"([^"]+)"', desktop))
builtin_ids.update({"terminal", "browser", "settings"})
missing_launches = sorted(qml_launches - builtin_ids)
if missing_launches:
    print(
        "QML launch IDs without a native/shell target: " + ", ".join(missing_launches),
        file=sys.stderr,
    )

if direct_url_calls:
    print(
        "QML must route web links through shell.openUrl/default browser; direct "
        "Qt.openUrlExternally found in: " + ", ".join(direct_url_calls),
        file=sys.stderr,
    )

required_tools = {
    "controlExecutable": "lunadashctl",
    "desktopExecutable": "lunadash-desktop",
    "updaterExecutable": "lunadash-update",
    "shellToolExecutable": "lunadash-shell-tool",
}
missing_tools = []
for prop, binary in required_tools.items():
    if prop not in shell or binary not in cmake_text:
        missing_tools.append(f"{prop}->{binary}")
if missing_tools:
    print(
        "QML tool bindings missing install/build targets: " + ", ".join(missing_tools),
        file=sys.stderr,
    )

bridge_ok = (
    "function openUrl(url)" in shell
    and 'command("open-url"' in shell
    and "open-url" in supported_methods
)
if not bridge_ok:
    print(
        "shell.openUrl must forward URLs to the compositor open-url action",
        file=sys.stderr,
    )

if missing_methods or missing_launches or direct_url_calls or missing_tools or not bridge_ok:
    raise SystemExit(1)

print("QML action review passed")
print("  compositor commands:", ", ".join(sorted(qml_methods)))
print("  launch targets:", ", ".join(sorted(qml_launches)))
print("  external web links: default-browser bridge")
