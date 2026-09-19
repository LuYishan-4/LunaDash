#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

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
update_state = (root / "qml/settings/components/UpdateState.qml").read_text(encoding="utf-8")
assert "cleanup" in update_state and "install-script" in update_state
assert "Qt.openUrlExternally" not in about
assert '"browser"' in defaults
browser = (root / "src/desktop/browser/Browser.cpp").read_text(encoding="utf-8")
assert "google-chrome-stable" in browser
assert "Browser::defaultCommand" in defaults
assert "function openUrl(url)" in shell
subprocess.run([sys.executable, str(root / "tests/update/test_installer.py")], check=True)
print("update/default-browser contract passed")
