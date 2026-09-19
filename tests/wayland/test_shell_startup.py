"""Run the complete shell on an isolated headless compositor (Arch CI)."""
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import tempfile
import time

build = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="lunadash-shell-") as directory:
    root = Path(directory)
    install = root / "install"
    subprocess.run(["cmake", "--install", str(build), "--prefix", str(install)], check=True, stdout=subprocess.DEVNULL)
    data = root / "data"
    services = data / "dbus-1/services"
    services.mkdir(parents=True)
    (services / "org.freedesktop.impl.portal.desktop.lunadash.service").write_text(
        "[D-BUS Service]\nName=org.freedesktop.impl.portal.desktop.lunadash\nExec=" +
        str(install / "libexec/xdg-desktop-portal-lunadash") + "\n")
    fixture_bin = root / "bin"
    fixture_bin.mkdir()
    ddc = fixture_bin / "ddcutil"
    ddc.write_text('#!/bin/sh\nif [ "$1" = detect ]; then\n printf "Display 1\\n I2C bus: /dev/i2c-7\\n Monitor: DEL:Fixture monitor:123\\n"\nelse\n printf "VCP 10 C 80 200\\n"\nfi\n')
    ddc.chmod(0o700)
    env = os.environ | {"XDG_RUNTIME_DIR": directory, "XDG_CONFIG_HOME": directory,
        "XDG_DATA_HOME": str(data), "XDG_DATA_DIRS": str(install / "share") + ":/usr/share", "XDG_STATE_HOME": directory, "XDG_CACHE_HOME": directory,
        "WLR_BACKENDS": "headless", "WLR_HEADLESS_OUTPUTS": "1", "WLR_RENDERER": "pixman",
        "QT_QPA_PLATFORM": "wayland", "QT_QUICK_BACKEND": "software",
        "LUDASH_DISABLE_XWAYLAND": "1", "LUDASH_SKIP_SETUP": "1", "LUNADASH_DISABLE_FCITX": "1",
        "QT_QPA_PLATFORMTHEME": "xdgdesktopportal", "GTK_USE_PORTAL": "1",
        "LUNADASH_PUBLISH_ACTIVATION_ENV": "1", "LUDASH_TEST_SETTINGS": "1", "PATH": str(fixture_bin) + os.pathsep + os.environ["PATH"]}
    for key in ("WAYLAND_DISPLAY", "DISPLAY", "DBUS_SESSION_BUS_ADDRESS"):
        env.pop(key, None)
    control = root / "lunadash-shell-test-control"
    def request(method="status", value=""):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(3)
            connection.connect(str(control))
            connection.sendall(json.dumps({"method":method,"value":value}).encode()+b"\n")
            data = b""
            while b"\n" not in data:
                chunk = connection.recv(65536)
                assert chunk and len(data)<2*1024*1024
                data += chunk
            return json.loads(data)
    log_path = build / "shell-startup.log"
    with log_path.open("w") as log:
        process = subprocess.Popen(["dbus-run-session", "--", str(install / "bin/lunadash-compositor"), "--socket", "lunadash-shell-test"], env=env, stdout=log, stderr=log, start_new_session=True)
        try:
            deadline = time.monotonic()+20
            saw_splash = False
            while True:
                assert process.poll() is None, log_path.read_text()
                state = request() if control.exists() else {}
                layers = state.get("layerNamespaces", [])
                saw_splash |= "lunadash-startup" in layers
                if saw_splash and "lunadash-wallpaper" in layers and "lunadash-panel" in layers and "lunadash-startup" not in layers:
                    break
                assert time.monotonic()<deadline, log_path.read_text()
                time.sleep(.04)
            assert str(install / "share/lunadash/shell/shell.qml") in log_path.read_text()
            pages = sorted(path.stem for path in Path("qml/settings/pages").glob("*.qml"))
            for language in ("en_US", "zh_TW", "zh_CN", "ja_JP"):
                assert "error" not in request("language", language)
                for page in pages:
                    marker = "Settings page loaded: " + page
                    previous_loads = log_path.read_text().count(marker)
                    result = request("open-settings", page)
                    assert "error" not in result, (page, result, log_path.read_text())
                    deadline = time.monotonic()+8
                    while ("lunadash-settings" not in request()["layerNamespaces"] or
                           log_path.read_text().count(marker) <= previous_loads):
                        assert time.monotonic()<deadline, log_path.read_text()
                        time.sleep(.1)
            # Ask an actual client on the compositor's private bus to activate
            # the frontend. Its FileChooser must be available without a timeout.
            portal_result = root / "portal-result"
            probe = fixture_bin / "portal-probe"
            probe.write_text("#!/bin/sh\ngdbus call --session --dest org.freedesktop.portal.Desktop "
                "--object-path /org/freedesktop/portal/desktop --method org.freedesktop.DBus.Properties.Get "
                "org.freedesktop.portal.FileChooser version > " + str(portal_result) + " 2>&1\n")
            probe.chmod(0o700)
            assert "error" not in request("launch-command", json.dumps([str(probe)]))
            deadline = time.monotonic()+8
            while not portal_result.exists() or "uint32" not in portal_result.read_text():
                assert time.monotonic()<deadline, log_path.read_text() + (portal_result.read_text() if portal_result.exists() else "")
                time.sleep(.1)
            text = log_path.read_text()
            for error in ("ReferenceError:", "TypeError:", "Cannot assign", "Binding loop detected", "Failed to load configuration", "Settings page failed to load", "NoReply", "Failed to create settings proxy", "Failed to create file chooser proxy"):
                assert error not in text, text
            print("Full shell startup passed: animated splash maps and closes, wallpaper/panel map, all installed settings pages load in four languages and FileChooser activates.")
        finally:
            try: request("quit")
            except (OSError, ValueError): pass
            try: process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGTERM)
                try: process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait(timeout=3)
