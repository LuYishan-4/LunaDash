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
# original wl_keyboard events through the seat.
assert "const bool inputMethodOwnsKeyboard" in input_cpp
guard = input_cpp[input_cpp.index("const bool inputMethodOwnsKeyboard") :]
guard = guard[: guard.index("wlr_seat_keyboard_notify_key") + 64]
assert "activeTextInput" in guard
assert "focused_surface" in guard
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
