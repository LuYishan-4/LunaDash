"""Exercise the real backend, frontend and chooser as separate processes.

Run under xvfb-run + dbus-run-session. No test-only autoaccept hook is used in
production code: xdotool activates the real dialog and its confirmation button.
"""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET

import dbus
from dbus.mainloop.glib import DBusGMainLoop
from gi.repository import GLib

ROOT = Path(__file__).resolve().parents[2]
BUILD = Path(sys.argv[1]).resolve()
BACKEND = BUILD / "xdg-desktop-portal-lunadash"
SERVICE = "org.freedesktop.impl.portal.desktop.lunadash"
DESKTOP = "/org/freedesktop/portal/desktop"
FILE_IFACE = "org.freedesktop.impl.portal.FileChooser"
REQUEST_IFACE = "org.freedesktop.portal.Request"
LOGDIR = BUILD / "portal-runtime"
LOGDIR.mkdir(exist_ok=True)
DBusGMainLoop(set_as_default=True)
bus = dbus.SessionBus()
context = GLib.MainContext.default()


def wait_for(predicate, message, timeout=12):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        while context.pending():
            context.iteration(False)
        result = predicate()
        if result:
            return result
        time.sleep(0.025)
    raise AssertionError(message)


def xdo(*args):
    return subprocess.run(["xdotool", *map(str, args)], check=True,
                          capture_output=True, text=True, timeout=4).stdout


def find_window(pid, title):
    result = subprocess.run(
        ["xdotool", "search", "--onlyvisible", "--pid", str(pid), "--name", title],
        capture_output=True, text=True, timeout=4)
    return result.stdout.splitlines()[0] if result.returncode == 0 else None


def confirm(window, path=None):
    xdo("windowfocus", "--sync", window)
    if path is not None:
        xdo("key", "--clearmodifiers", "ctrl+l")
        xdo("type", "--clearmodifiers", "--delay", 1, "--", path)
        xdo("key", "Return")
    geometry = dict(line.split("=", 1) for line in
                    xdo("getwindowgeometry", "--shell", window).splitlines()
                    if "=" in line)
    # The existing template keeps Share/Open/Save in the lower right footer.
    xdo("mousemove", "--window", window, int(geometry["WIDTH"]) - 100,
        int(geometry["HEIGHT"]) - 32, "click", 1)


def response_call(method, *args):
    answers, errors = [], []
    method(*args, reply_handler=lambda *value: answers.append(value),
           error_handler=lambda error: errors.append(str(error)), timeout=15)
    return answers, errors


def checked_wait(answers, errors):
    wait_for(lambda: answers or errors, "No D-Bus response received")
    assert not errors, errors
    return answers[0]


processes, logs = [], []
with tempfile.TemporaryDirectory(prefix="lunadash-portal-runtime-") as temp:
    base = Path(temp)
    env = os.environ | {
        "XDG_CONFIG_HOME": str(base / "config"),
        "XDG_DATA_HOME": str(base / "data"),
        "XDG_CACHE_HOME": str(base / "cache"),
        "XDG_CURRENT_DESKTOP": "LunaDash",
        "XDG_SESSION_TYPE": "x11", "QT_QPA_PLATFORM": "xcb",
        "QT_QPA_PLATFORMTHEME": "xdgdesktopportal", "GTK_USE_PORTAL": "1",
        "QT_IM_MODULE": "compose", "QT_IM_MODULES": "compose",
        "LUDASH_LANGUAGE": "en_US",
        "QT_LOGGING_TO_CONSOLE": "1",
    }
    env.pop("LUNADASH_SCREENCAST_CHOOSER_AUTOPICK", None)
    fixture = base / "picked file.txt"
    fixture.write_text("portal integration fixture", encoding="utf-8")

    def launch(name, command, **kwargs):
        log = (LOGDIR / (name + ".log")).open("w+")
        logs.append(log)
        process = subprocess.Popen(command, env=env, stdout=log,
                                   stderr=log, **kwargs)
        processes.append(process)
        return process

    try:
        backend = launch("backend", [str(BACKEND)])
        wait_for(lambda: bus.name_has_owner(SERVICE), "Backend did not acquire its name")
        exported = bus.get_object(SERVICE, DESKTOP, introspect=False)
        xml = dbus.Interface(exported, "org.freedesktop.DBus.Introspectable").Introspect()
        (LOGDIR / "backend-introspection.xml").write_text(str(xml), encoding="utf-8")
        interfaces = {entry.attrib["name"]: entry for entry in ET.fromstring(xml).findall("interface")}
        file_api = interfaces[FILE_IFACE]
        for name in ("OpenFile", "SaveFile", "SaveFiles"):
            method = next(item for item in file_api.findall("method") if item.attrib["name"] == name)
            inputs = "".join(arg.attrib["type"] for arg in method.findall("arg") if arg.attrib.get("direction", "in") == "in")
            outputs = "".join(arg.attrib["type"] for arg in method.findall("arg") if arg.attrib.get("direction") == "out")
            assert inputs == "osssa{sv}" and outputs == "ua{sv}", (name, inputs, outputs)
        assert int(dbus.Interface(exported, "org.freedesktop.DBus.Properties").Get(FILE_IFACE, "version")) == 4
        signal = interfaces["org.freedesktop.impl.portal.Settings"].find("signal[@name='SettingChanged']")
        assert signal is not None and "".join(arg.attrib["type"] for arg in signal.findall("arg")) == "ssv"
        print("Cold-start introspection: FileChooser v4 signatures and Settings signal exported", flush=True)
        impl = dbus.Interface(exported, FILE_IFACE)
        # A direct C++ call cannot catch a broken Qt-generated D-Bus signature.
        for mode in ("OpenFile", "SaveFile", "OpenFile"):
            handle = dbus.ObjectPath("/org/freedesktop/portal/desktop/request/test/direct")
            options = dbus.Dictionary({"current_folder": dbus.ByteArray(os.fsencode(base) + b"\0")}, signature="sv")
            title = "LunaDash integration " + mode
            answers, errors = response_call(getattr(impl, mode), handle, "", "", title, options)
            window = wait_for(lambda: find_window(backend.pid, title) or errors,
                              f"{mode} did not open a real dialog")
            assert not errors, errors
            target = str(fixture if mode == "OpenFile" else base / "new file.txt")
            confirm(window, target)
            response, results = checked_wait(answers, errors)
            assert int(response) == 0, (mode, response, results)
            assert list(results["uris"]) == [Path(target).as_uri()], results
        print("Backend OpenFile/SaveFile: actual dialogs returned matching URIs", flush=True)

        # Cancellation must work over D-Bus while a file dialog is open.
        answers, errors = response_call(impl.OpenFile, handle, "", "", "LunaDash cancel test", options)
        wait_for(lambda: find_window(backend.pid, "LunaDash cancel test"), "Cancel dialog missing")
        dbus.Interface(bus.get_object(SERVICE, handle), "org.freedesktop.impl.portal.Request").Close()
        response, results = checked_wait(answers, errors)
        assert int(response) == 1 and not results, (response, results)
        assert backend.poll() is None
        print("Backend Request.Close: returned cancellation, process still alive", flush=True)
        assert not bus.name_has_owner("org.freedesktop.portal.Desktop"), (
            "The backend recursively activated the public portal before test setup"
        )
        # Do not let the direct calls warm up the backend before frontend testing.
        backend.terminate()
        backend.wait(timeout=3)
        wait_for(lambda: not bus.name_has_owner(SERVICE), "Backend name was not released")
        backend = launch("backend-cold-frontend", [str(BACKEND)])
        wait_for(lambda: bus.name_has_owner(SERVICE), "Cold frontend backend did not start")

        # The frontend runs with actual routing files, not a fake success stub.
        definitions = base / "portals"
        definitions.mkdir()
        shutil.copy2(ROOT / "data/portal/lunadash.portal", definitions)
        (definitions / "lunadash-portals.conf").write_text(
            "[preferred]\ndefault=lunadash\n"
            "org.freedesktop.impl.portal.FileChooser=lunadash\n"
            "org.freedesktop.impl.portal.Settings=lunadash\n", encoding="utf-8")
        env["XDG_DESKTOP_PORTAL_DIR"] = str(definitions)
        frontend_binary = next((p for p in (
            "/usr/libexec/xdg-desktop-portal", "/usr/lib/xdg-desktop-portal")
            if Path(p).is_file()), None)
        assert frontend_binary, "xdg-desktop-portal is required"
        frontend = launch("frontend", [frontend_binary, "--replace", "--verbose"])
        wait_for(lambda: bus.name_has_owner("org.freedesktop.portal.Desktop"),
                 "Frontend did not start", 20)
        owner = bus.get_name_owner("org.freedesktop.portal.Desktop")
        owner_pid = dbus.Interface(bus.get_object("org.freedesktop.DBus", "/org/freedesktop/DBus"),
                                   "org.freedesktop.DBus").GetConnectionUnixProcessID(owner)
        assert int(owner_pid) == frontend.pid, ("Unexpected frontend owner", owner_pid)
        assert frontend.poll() is None
        replies = {}
        bus.add_signal_receiver(lambda code, result, path: replies.update({path: (code, result)}),
                                "Response", REQUEST_IFACE, path_keyword="path")
        public = dbus.Interface(bus.get_object("org.freedesktop.portal.Desktop", DESKTOP),
                                "org.freedesktop.portal.FileChooser")
        path = public.OpenFile("", "LunaDash frontend test", options)
        window = wait_for(lambda: find_window(backend.pid, "LunaDash frontend test") or replies.get(path),
                          "Frontend did not invoke the LunaDash backend")
        assert path not in replies, replies
        confirm(window, str(fixture))
        wait_for(lambda: path in replies, "Frontend did not emit Request.Response")
        assert int(replies[path][0]) == 0, replies[path]
        assert list(replies[path][1]["uris"]) == [fixture.as_uri()], replies[path]
        print("Frontend FileChooser: full bus -> backend UI -> Response path passed", flush=True)

        settings = dbus.Interface(bus.get_object("org.freedesktop.portal.Desktop", DESKTOP),
                                  "org.freedesktop.portal.Settings")
        assert int(settings.Read("org.freedesktop.appearance", "color-scheme")) in (1, 2)
        print("Frontend Settings: desktop color-scheme is readable", flush=True)

        # Run the real chooser UI, not its old AUTOPICK testing shortcut. The raw
        # output must preserve labels, including meaningful whitespace.
        for index, (raw, accept) in enumerate((
            ("Monitor: HEADLESS-1 Display with trailing space  ", True),
            ("Window: editor \u5de5\u4f5c\u5340 (id-1)  ", True),
            ("Monitor: HEADLESS-1 Display with trailing space  ", False),
        )):
            chooser_log = (LOGDIR / f"chooser-{index}.log").open("w+")
            logs.append(chooser_log)
            chooser = subprocess.Popen([str(BACKEND), "--screencast-chooser"],
                                       env=env, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=chooser_log, text=True)
            processes.append(chooser)
            chooser.stdin.write(raw + "\n")
            chooser.stdin.close()
            window = wait_for(lambda: find_window(chooser.pid, "Share your screen"),
                              "Screen chooser UI did not appear")
            xdo("windowfocus", "--sync", window)
            if accept:
                xdo("key", "Return")
            else:
                xdo("key", "Escape")
            chooser.wait(timeout=8)
            output = chooser.stdout.read()
            assert chooser.returncode == 0, chooser.returncode
            assert output == (raw + "\n" if accept else ""), repr(output)
        print("Screen chooser: real confirmation/cancellation and lossless UTF-8 labels passed", flush=True)
        for name in ("backend.log", "backend-cold-frontend.log"):
            assert "Unregistered input type" not in (LOGDIR / name).read_text(), name
    finally:
        for process in reversed(processes):
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=3)
        for log in logs:
            log.flush()
            log.seek(0)
            print(f"--- {log.name} ---\n{log.read()}", file=sys.stderr)
            log.close()
