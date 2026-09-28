"""Lock keyboard/launcher ownership rules that keep QML and apps typable."""

from pathlib import Path

root = Path(__file__).resolve().parents[2]
input_cpp = (root / "src/compositor/input/Input.cpp").read_text(encoding="utf-8")
compositor = (root / "src/compositor/wayland/WaylandCompositor.cpp").read_text(
    encoding="utf-8"
)
launcher = (root / "qml/launcher/Launcher.qml").read_text(encoding="utf-8")
shell = (root / "qml/shell.qml").read_text(encoding="utf-8")

# An input-method keyboard grab is valid only while an enabled text-input-v3
# client actually owns focus. Otherwise QML layer-shell TextFields need the
# original wl_keyboard events through the seat. The bridge predicate is shared
# with virtual-keyboard routing, so verify the ownership inputs where the
# predicate is defined instead of requiring them to be duplicated below.
key_handler = input_cpp[input_cpp.index(
    "void WaylandCompositor::Impl::handleKeyboardKey"
):input_cpp.index(
    "void WaylandCompositor::Impl::handleKeyboardModifiers"
)]
assert "const bool inputMethodBridgeActive" in key_handler
bridge = key_handler[key_handler.index("const bool inputMethodBridgeActive") :]
bridge = bridge[: bridge.index("const bool inputMethodVirtual")]
assert "activeTextInput" in bridge
assert "focused_surface" in bridge
assert "keyboard_grab" in bridge
assert "const bool inputMethodOwnsKeyboard" in key_handler
guard = key_handler[key_handler.index("const bool inputMethodOwnsKeyboard") :]
guard = guard[: guard.index("wlr_seat_keyboard_notify_key") + 64]
assert "inputMethodBridgeActive" in guard
assert "!inputMethodVirtual" in guard
assert "keyboard_grab" in guard

# Either activation source owns the search field while the launcher is open.
assert "WlrLayershell.keyboardFocus: opened" in launcher
assert "WlrKeyboardFocus.Exclusive : WlrKeyboardFocus.None" in launcher
assert 'launcherOpenSource === "keyboard"' not in launcher
assert "Component.onCompleted: if (opened)" in launcher
assert "launcher.focusSearch(false)" in launcher

# Bare Meta is a compositor toggle, not an always-open command.
assert 'setLauncherVisible(!launcherVisible_)' in compositor
assert 'handleShortcut("launchLauncher")' in input_cpp

# Never redirect the launcher's first typed key to a previously focused app.
assert "const bool openLauncher = state->metaTapPending" in input_cpp
assert "self->q->setLauncherVisible(false)" not in input_cpp
assert "WindowSwitcher::Scope::Workspaces" in input_cpp
assert "WLR_MODIFIER_LOGO : WLR_MODIFIER_ALT" in input_cpp
assert "windowSwitcher_->dismissPopups()" in input_cpp

# Shell interaction state carries both serial and desired visibility so a
# second Meta tap closes the launcher immediately.
assert "applyLauncherSignal(serial, visible)" in shell
assert "launcherOpen = Boolean(visible)" in shell
assert 'command("launcher-visible", launcherOpen ? "true" : "false")' in shell

print("Input ownership contract passed: QML typing, app typing and Meta launcher toggle.")


# The input method's own virtual keyboard is a return path and must never
# become the seat's physical modifier owner. While the input-method grab is
# active, synchronize the complete physical modifier snapshot before a
# forwarded virtual key so Ctrl/Shift/Alt as well as CapsLock/NumLock survive.
modifiers_handler = input_cpp[input_cpp.index(
    "void WaylandCompositor::Impl::handleKeyboardModifiers"
):input_cpp.index(
    "void WaylandCompositor::Impl::handleKeyboardDestroy"
)]
assert "const bool inputMethodVirtual" in modifiers_handler
virtual_modifier = modifiers_handler[modifiers_handler.index(
    "if (inputMethodVirtual)"
):modifiers_handler.index(
    "wlr_seat_set_keyboard(self->seat, state->keyboard)"
)]
assert "restorePreferredKeyboard()" in virtual_modifier
assert "inputMethodBridgeActive" in modifiers_handler
assert "&physical->modifiers" in virtual_modifier
assert "modifiers.locked = physical->modifiers.locked" not in modifiers_handler
# The physical set_keyboard call follows the IME virtual branch. That branch
# must return before execution can reach it.
assert "return;" in virtual_modifier
key_owner = input_cpp[input_cpp.index("auto *keyboard = state->keyboard;"):]
key_owner = key_owner[: key_owner.index("const uint32_t keycode")]
assert "if (state->virtualKeyboard && !inputMethodVirtual)" in key_owner
assert "copyKeyboardLocks(keyboard, physical)" in key_owner
assert "if (inputMethodVirtual)" in key_owner
assert "restorePreferredKeyboard()" in key_owner
assert "&physical->modifiers" in key_owner
