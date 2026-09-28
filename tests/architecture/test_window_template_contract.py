"""Prevent removed window/plugin architecture from growing back."""

from pathlib import Path

root = Path(__file__).resolve().parents[2]

for relative in (
    "src/compositor/input/TiledPointer.cpp",
    "src/compositor/input/WindowSwitch.cpp",
    "src/compositor/session/ClientLaunch.cpp",
    "src/compositor/session/ClientLaunch.hpp",
    "src/compositor/animation/SceneWindowAnimations.cpp",
    "src/compositor/animation/SceneWindowAnimations.hpp",
    "src/desktop/system/SystemMonitor.cpp",
    "src/desktop/system/SystemMonitor.hpp",
    "src/desktop/app/ApplicationCatalog.cpp",
    "src/desktop/app/ApplicationCatalog.hpp",
    "src/desktop/launcher/Launcher.cpp",
    "src/desktop/launcher/Launcher.hpp",
):
    assert not (root / relative).exists(), f"legacy source returned: {relative}"

layout = (root / "src/compositor/layout/WindowLayout.hpp").read_text(encoding="utf-8")
template = (root / "src/compositor/window/WindowTemplate.hpp").read_text(encoding="utf-8")
manager = (root / "src/compositor/plugins/PluginManager.cpp").read_text(encoding="utf-8")
catalog = (root / "src/config/plugins/PluginCatalog.cpp").read_text(encoding="utf-8")
validator = (root / "cmake/plugins/ValidatePlugin.py").read_text(encoding="utf-8")

# Base layout is lifecycle/state only. Strategy vocabulary belongs to template
# actions/capabilities and concrete implementations.
for token in (
    "WindowLayoutMode",
    "focusLeft(",
    "focusRight(",
    "focusUp(",
    "focusDown(",
    "groupWith(",
    "expel(",
    "swapWindows(",
    "insertBeside(",
    "resizeHeight(",
    "moveSingle(",
    "reorder(",
    "center(",
):
    assert token not in layout, f"strategy method leaked into WindowLayout: {token}"
assert "performAction(const QString &action" in layout

# Template behavior is capability/action driven rather than enum branches.
assert "WindowPointerTemplate" not in template
assert "WindowActivationTemplate" not in template
assert "layoutActions" in template
assert "activationTogglesMaximize" in template

# SDK 2 no longer accepts or emits the legacy layoutMode property.
assert 'manifest.value("layoutMode")' not in manager
assert 'value("layoutMode")' not in catalog
assert '"layoutMode was removed; use windowTemplate"' in validator

# Plugin settings are automatic and deliberately limited to the four shared UI
# widgets. Plugins do not ship custom settings forms for basic options.
assert 'allowed_controls = {"toggle", "select", "number", "slider"}' in validator

print("Window/plugin architecture contract passed: no legacy modes or hardcoded base actions.")


# Client titlebar move requests are accepted for ordinary tiled windows and
# exchange slots on drop. The modifier-assisted path remains a fallback.
pointer = (root / "src/compositor/window/WindowPointer.cpp").read_text(
    encoding="utf-8"
)
assert "if (edges == 0)" in pointer
assert "windowAllowsPointerInteraction" in pointer
assert "pointerStartGeometry = client->geometry" in pointer
assert "const bool directMove = pointerClientGrab" in pointer
assert '"swap"' in pointer
assert "pointerStartGeometry.topLeft()" in pointer


# Generic virtual-keyboard clients (recorders/remappers) can be recreated with
# an empty locked mask. Physical XKB state is the persistent CapsLock/NumLock
# owner: inherit it before processing a virtual key and write lock changes back
# from the modifiers callback. Destruction keeps a final fallback sync.
input_source = (root / "src/compositor/input/Input.cpp").read_text(
    encoding="utf-8"
)
assert "bool copyKeyboardLocks(" in input_source
assert "copyKeyboardLocks(keyboard, physical)" in input_source
assert "copyKeyboardLocks(physical, state->keyboard)" in input_source
assert "XKB_MOD_NAME_CAPS" in input_source
assert "XKB_MOD_NAME_NUM" in input_source
