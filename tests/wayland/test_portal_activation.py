"""The FileChooser backend must acquire its name without querying its frontend."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

if len(sys.argv) == 2:
    with tempfile.TemporaryDirectory(prefix="lunadash-portal-") as directory:
        root = Path(directory)
        services = root / "data/dbus-1/services"
        services.mkdir(parents=True)
        fake = root / "unresponsive-frontend.py"
        fake.write_text("from pathlib import Path\nimport time\nPath(" + repr(str(root / "queried")) + ").touch()\ntime.sleep(60)\n")
        (services / "org.freedesktop.portal.Desktop.service").write_text(
            "[D-BUS Service]\nName=org.freedesktop.portal.Desktop\nExec=" + sys.executable + " " + str(fake) + "\n")
        env = os.environ | {"XDG_DATA_HOME": str(root / "data"), "XDG_CONFIG_HOME": directory,
            "XDG_RUNTIME_DIR": directory, "QT_QPA_PLATFORM": "offscreen", "QT_IM_MODULE": "none",
            "QT_QPA_PLATFORMTHEME": "xdgdesktopportal", "GTK_USE_PORTAL": "1"}
        subprocess.run(["dbus-run-session", "--", sys.executable, __file__, sys.argv[1], directory], env=env, check=True, timeout=12)
else:
    root = Path(sys.argv[2])
    process = subprocess.Popen([str(Path(sys.argv[1]).resolve() / "xdg-desktop-portal-lunadash")], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    try:
        deadline = time.monotonic()+5
        while True:
            names = subprocess.check_output(["dbus-send", "--session", "--print-reply", "--dest=org.freedesktop.DBus", "/", "org.freedesktop.DBus.ListNames"], text=True, timeout=2)
            if '"org.freedesktop.impl.portal.desktop.lunadash"' in names:
                break
            assert process.poll() is None, process.stdout.read()
            assert time.monotonic()<deadline, "Portal backend blocked before acquiring its name"
            time.sleep(.05)
        assert not (root / "queried").exists(), "Backend recursively activated its frontend"
        print("FileChooser activation passed with an unresponsive frontend and inherited portal theme.")
    finally:
        process.terminate()
        try: process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=2)
