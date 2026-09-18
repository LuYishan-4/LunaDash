"""Reject QtWayland compositor/server dependencies in the wlroots core."""

from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
roots = [
    root / "src" / "compositor",
    root / "src" / "desktop" / "InputSettings",
    root / "cmake",
]
patterns = (
    re.compile(r"QtWaylandCompositor"),
    re.compile(r"\bQWayland(?:Compositor|Seat|Surface|Output|Keyboard|Pointer|Quick|Xdg)"),
    re.compile(r"WaylandCompositorPrivate"),
)
violations = []
for base in roots:
    for path in base.rglob("*"):
        if not path.is_file() or path.suffix not in {".c", ".cpp", ".h", ".hpp", ".cmake"}:
            continue
        text = path.read_text(encoding="utf-8")
        for pattern in patterns:
            if pattern.search(text):
                violations.append(f"{path.relative_to(root)}: {pattern.pattern}")

assert not violations, "QtWayland compositor dependency remains:\n" + "\n".join(violations)

cmake = (root / "cmake" / "LunaDashMain.cmake").read_text(encoding="utf-8")
assert "PkgConfig::WLROOTS" in cmake
assert "WLR_USE_UNSTABLE" in cmake
assert "Qt6::WaylandCompositor" not in cmake
print("wlroots core check passed: no QtWayland compositor/server API remains")
