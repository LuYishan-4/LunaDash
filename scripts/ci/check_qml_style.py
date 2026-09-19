#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
QML = ROOT / "qml"
CANONICAL = QML / "components"
violations = []

# Application QML must consume the LunaDash design-system controls instead of
# styling Qt Quick Controls ad-hoc. Canonical components are the only exception.
raw_controls = re.compile(r"(?m)^\s*(Button|ComboBox|TextField|Switch|Slider)\s*\{")
hard_font = re.compile(r"font\.family\s*:\s*[\"']")
literal_icon = re.compile(
    r'LineIcon\s*\{[^{}]*?\bname\s*:\s*"([A-Za-z0-9_-]+)"',
    re.S,
)
icon_source = (CANONICAL / "LineIcon.qml").read_text(encoding="utf-8")
known_icons = set(re.findall(r'^\s*([A-Za-z0-9_-]+)\s*:', icon_source, re.M))

for manifest in sorted(QML.rglob("qmldir")):
    declared = set(re.findall(r"\b([A-Za-z][A-Za-z0-9_]*\.qml)\b", manifest.read_text(encoding="utf-8")))
    for component in sorted(manifest.parent.glob("*.qml")):
        if component.name[0].isupper() and component.name not in declared:
            violations.append(f"{component.relative_to(ROOT)}: missing from {manifest.relative_to(ROOT)}")
    for filename in sorted(declared):
        if not (manifest.parent / filename).is_file():
            violations.append(f"{manifest.relative_to(ROOT)}: declared component does not exist: {filename}")

for path in sorted(QML.rglob("*.qml")):
    text = path.read_text(encoding="utf-8")
    rel = path.relative_to(ROOT)
    if CANONICAL not in path.parents:
        for match in raw_controls.finditer(text):
            line = text.count("\n", 0, match.start()) + 1
            violations.append(f"{rel}:{line}: raw {match.group(1)}; use a qml/components design-system control")
    for match in hard_font.finditer(text):
        line = text.count("\n", 0, match.start()) + 1
        violations.append(f"{rel}:{line}: hard-coded font family; use Theme.font")
    if path != CANONICAL / "LineIcon.qml":
        for match in literal_icon.finditer(text):
            icon_name = match.group(1)
            if icon_name not in known_icons:
                line = text.count("\n", 0, match.start()) + 1
                violations.append(
                    f"{rel}:{line}: unknown LineIcon name '{icon_name}'"
                )

if violations:
    print("QML design-system review failed:")
    print("\n".join(f"  {item}" for item in violations))
    sys.exit(1)
print("QML design-system review passed: shared controls, icons and font policy are consistent.")
