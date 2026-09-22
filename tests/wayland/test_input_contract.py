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

# Mouse-open launcher must leave the application keyboard owner untouched.
assert "opened && shell.launcherKeyboardActive" in launcher
assert "WlrKeyboardFocus.Exclusive" in launcher
assert 'launcherOpenSource === "keyboard"' in launcher

# Bare Meta is a compositor toggle, not an always-open command.
assert 'setLauncherVisible(!launcherVisible_)' in compositor
assert 'handleShortcut("launchLauncher")' in input_cpp

# If an app already owns keyboard focus, its first printable key dismisses the
# launcher but is not marked handled/returned before normal key forwarding.
close_at = input_cpp.index("self->q->setLauncherVisible(false)")
forward_at = input_cpp.index("wlr_seat_keyboard_notify_key", close_at)
assert forward_at > close_at

# Shell interaction state carries both serial and desired visibility so a
# second Meta tap closes the launcher immediately.
assert "applyLauncherSignal(serial, visible)" in shell
assert "launcherOpen = Boolean(visible)" in shell
assert 'command("launcher-visible", launcherOpen ? "true" : "false")' in shell

print("Input ownership contract passed: QML typing, app typing and Meta launcher toggle.")
