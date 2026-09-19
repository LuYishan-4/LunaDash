#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[2]
updater = (root / "scripts/lunadash-update").read_text(encoding="utf-8")
installer = (root / "scripts/install-session.sh").read_text(encoding="utf-8")
about = (root / "qml/settings/pages/about.qml").read_text(encoding="utf-8")
defaults = (root / "src/desktop/app/DefaultApplications.cpp").read_text(encoding="utf-8")
shell = (root / "qml/shell.qml").read_text(encoding="utf-8")

assert "status_pid=$$" in updater, "progress JSON must contain the updater PID"
assert "git clone" in updater
assert 'bash "$src/scripts/install-session.sh" --skip-deps' in updater
assert "LUDASH_INSTALL_PROGRESS_FILE" in updater
assert 'rm -rf "$work"' in updater
assert 'cmake --install "$build"' not in updater
assert "report_progress" in installer
assert "LUDASH_PREFER_PKEXEC" in installer
assert "pkexec" in installer
assert "cleanup" in about and "install-script" in about
assert "Qt.openUrlExternally" not in about
assert '"browser"' in defaults
browser = (root / "src/desktop/browser/Browser.cpp").read_text(encoding="utf-8")
assert "google-chrome-stable" in browser
assert "Browser::defaultCommand" in defaults
assert "function openUrl(url)" in shell
print("update/default-browser contract passed")
