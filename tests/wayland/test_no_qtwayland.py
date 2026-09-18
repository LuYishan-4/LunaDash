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

compat = (root / "src" / "compositor" / "wlroots" / "WlrootsCompat.hpp").read_text(
    encoding="utf-8"
)
compositor = (
    root
    / "src"
    / "compositor"
    / "WaylandCompositor"
    / "WaylandCompositor.cpp"
).read_text(encoding="utf-8")

assert "xdgToplevelDestroySignal" in compat
assert re.search(
    r"#if WLR_VERSION_MINOR < 20.*return &surface->events\.destroy;"
    r".*#else.*return &toplevel->events\.destroy;",
    compat,
    re.S,
), "xdg-toplevel destroy compatibility must preserve wlroots <0.20 and >=0.20 lifetimes"
assert "WlrootsCompat::xdgToplevelDestroySignal(surface, toplevel)" in compositor

destroy_start = compositor.index("static void handleToplevelDestroy")
destroy_end = compositor.index("static void handleNewLayerSurface", destroy_start)
destroy_block = compositor[destroy_start:destroy_end]
for listener in (
    "map",
    "unmap",
    "commit",
    "destroy",
    "setTitle",
    "setAppId",
    "setParent",
    "requestMinimize",
    "requestMaximize",
    "requestFullscreen",
):
    assert (
        f"detachListener(state->{listener});" in destroy_block
    ), f"toplevel teardown must detach {listener} before wlroots frees the role"

print(
    "wlroots core check passed: no QtWayland compositor/server API remains; "
    "xdg-toplevel lifecycle is version-safe"
)
