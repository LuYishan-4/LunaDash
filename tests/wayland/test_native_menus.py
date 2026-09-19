"""Exercise a GTK context menu with real input through an isolated Xvfb host."""
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time

build = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="lunadash-menu-") as directory:
    root = Path(directory)
    env = os.environ | {"XDG_RUNTIME_DIR": directory, "XDG_CONFIG_HOME": directory,
        "XDG_STATE_HOME": directory, "XDG_DATA_HOME": directory,
        "WLR_BACKENDS": "x11", "WLR_X11_OUTPUTS": "1", "WLR_RENDERER": "pixman",
        "LIBGL_ALWAYS_SOFTWARE": "1", "LUDASH_DISABLE_XWAYLAND": "1",
        "LUDASH_SKIP_SETUP": "1", "LUNADASH_DISABLE_FCITX": "1",
        "QT_QPA_PLATFORMTHEME": "generic", "GTK_USE_PORTAL": "0"}
    env.pop("WAYLAND_DISPLAY", None)
    control = root / "lunadash-menu-test-control"
    def request(method="status", value=""):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(3)
            connection.connect(str(control))
            connection.sendall(json.dumps({"method": method, "value": value}).encode()+b"\n")
            data = b""
            while b"\n" not in data:
                chunk = connection.recv(65536)
                assert chunk and len(data) < 2*1024*1024
                data += chunk
            return json.loads(data)
    marker = root / "activated"
    fixture = root / "gtk-menu.py"
    fixture.write_text('''import gi
from pathlib import Path
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk
window = Gtk.Window(title="LunaDash GTK menu fixture")
window.set_default_size(600, 400)
text = Gtk.TextView()
text.get_buffer().set_text("Right click to open the native GTK context menu.")
def populate(widget, menu):
    item = Gtk.MenuItem(label="Regression action")
    item.connect("activate", lambda *_: Path(''' + repr(str(marker)) + ''').write_text("activated"))
    menu.append(item)
    item.show()
text.connect("populate-popup", populate)
window.add(text)
window.show_all()
Gtk.main()
''')
    def wait_for(predicate, message):
        deadline = time.monotonic()+8
        while True:
            assert process.poll() is None, log_path.read_text()
            try:
                value = predicate()
                if value:
                    return value
            except (OSError, ValueError):
                pass
            assert time.monotonic()<deadline, message + "\n" + log_path.read_text()
            time.sleep(.05)
    log_path = build / "native-menus.log"
    with log_path.open("w") as log:
        process = subprocess.Popen([str(build / "lunadash-compositor"), "--no-shell", "--socket", "lunadash-menu-test"], env=env, stdout=log, stderr=log)
        try:
            wait_for(lambda: control.exists(), "No control socket")
            request("launch-command", json.dumps(["python3", str(fixture)]))
            def mapped_client():
                return next((c for c in request()["clients"] if c["mapped"] and c["title"] == "LunaDash GTK menu fixture"), None)
            client = wait_for(mapped_client, "GTK client did not map")
            host = subprocess.check_output(["xdotool", "search", "--onlyvisible", "--name", ".*"], env=env, text=True).splitlines()[-1]
            time.sleep(.4)
            for attempt in range(2):
                subprocess.run(["xdotool", "mousemove", "--window", host, str(client["x"]+80), str(client["y"]+80), "click", "3"], env=env, check=True)
                wait_for(lambda: request()["xdgPopupCount"] > 0, "GTK right click did not create a popup")
                # Keep the popup alive across toplevel focus/configure events.
                time.sleep(.4)
                assert request()["xdgPopupCount"] > 0, "GTK popup was immediately dismissed"
                subprocess.run(["xdotool", "key", "End", "Return"], env=env, check=True)
                wait_for(marker.exists, "Context-menu action was not delivered")
                marker.unlink()
                wait_for(lambda: request()["xdgPopupCount"] == 0, "GTK menu did not close")
            print("GTK native context menu passed: right click, persistent popup, keyboard selection, action and reopen.")
        finally:
            try: request("quit")
            except (OSError, ValueError): pass
            try: process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)
