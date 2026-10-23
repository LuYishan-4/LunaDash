"""Exercise the real portal backend, frontend, and Wayland source chooser.

Run under xvfb-run + dbus-run-session. Both Slint portal pickers run against an
isolated headless wlroots compositor and receive real virtual-keyboard events.
No picker shortcut bypass is used.
"""
import os
import json
from pathlib import Path
import shutil
import socket
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


def find_client(title):
    state = current_state()
    if not state:
        return None
    return next((client["id"] for client in state.get("clients", [])
                 if client.get("mapped") and client.get("title") == title), None)


def enter_manual_path(client_id, path):
    control_request("focus", client_id)
    subprocess.run(["wtype", "-s", path], env=wayland_env,
                   check=True, timeout=5)
    subprocess.run(["wtype", "-k", "Return"], env=wayland_env,
                   check=True, timeout=5)


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

    control_path = str(base / "portal-runtime-control")
    wayland_name = "portal-runtime"
    wayland_env = env | {
        "WAYLAND_DISPLAY": wayland_name,
        "LUNADASH_CONTROL": control_path,
        "XDG_SESSION_TYPE": "wayland",
    }

    def control_request(method="status", value=""):
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection:
            connection.settimeout(2)
            connection.connect(control_path)
            request = json.dumps({"method": method, "value": str(value)}).encode() + b"\n"
            connection.sendall(request)
            response = bytearray()
            while b"\n" not in response:
                chunk = connection.recv(65536)
                if not chunk or len(response) > 4 * 1024 * 1024:
                    raise RuntimeError("Invalid compositor control response")
                response.extend(chunk)
        return json.loads(bytes(response).split(b"\n", 1)[0])

    def current_state():
        try:
            return control_request()
        except (OSError, RuntimeError, json.JSONDecodeError):
            return None

    def launch(name, command, environment=env, **kwargs):
        log = (LOGDIR / (name + ".log")).open("w+")
        logs.append(log)
        process = subprocess.Popen(command, env=environment, stdout=log,
                                   stderr=log, **kwargs)
        processes.append(process)
        return process

    try:
        compositor_env = wayland_env | {
            "WLR_BACKENDS": "headless",
            "WLR_HEADLESS_OUTPUTS": "1",
            "WLR_RENDERER": "pixman",
            "LUDASH_SKIP_SETUP": "1",
            "LUDASH_DISABLE_XWAYLAND": "1",
        }
        compositor = launch("compositor", [str(BUILD / "lunadash-compositor"),
            "--no-shell", "--socket", wayland_name], environment=compositor_env)
        def compositor_ready():
            state = current_state()
            return state if state and state.get("display", {}).get("width", 0) > 0 else None

        wait_for(compositor_ready, "Headless Wayland compositor did not become ready")

        backend = launch("backend", [str(BACKEND)], environment=wayland_env)
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
            client_id = wait_for(lambda: find_client(title) or errors,
                                 f"{mode} did not map its Slint Wayland picker")
            assert not errors, errors
            target = str(fixture if mode == "OpenFile" else base / "new file.txt")
            if mode == "OpenFile" and title == "LunaDash integration OpenFile":
                evidence = BUILD / "ci-evidence" / "portal-filepicker.png"
                evidence.parent.mkdir(parents=True, exist_ok=True)
                control_request("focus", client_id)
                subprocess.run(["grim", str(evidence)], env=wayland_env,
                               check=True, timeout=12)
            enter_manual_path(client_id, target)
            response, results = checked_wait(answers, errors)
            assert int(response) == 0, (mode, response, results)
            assert list(results["uris"]) == [Path(target).as_uri()], results
        print("Backend OpenFile/SaveFile: actual dialogs returned matching URIs", flush=True)

        savefiles_options = dbus.Dictionary({
            "current_folder": dbus.ByteArray(os.fsencode(base) + b"\0"),
            "files": dbus.Array([dbus.ByteArray(b"picked file.txt\0")],
                                 signature="ay"),
        }, signature="sv")
        savefiles_handle = dbus.ObjectPath(
            "/org/freedesktop/portal/desktop/request/test/savefiles")
        answers, errors = response_call(
            impl.SaveFiles, savefiles_handle, "", "", "LunaDash SaveFiles test",
            savefiles_options)
        client_id = wait_for(lambda: find_client("LunaDash SaveFiles test") or errors,
                             "SaveFiles did not map its Slint Wayland picker")
        assert not errors, errors
        enter_manual_path(client_id, str(base))
        response, results = checked_wait(answers, errors)
        expected = base / "picked file (1).txt"
        assert int(response) == 0 and list(results["uris"]) == [expected.as_uri()], (
            response, results)
        print("Backend SaveFiles: selected folder and collision-safe URI passed", flush=True)

        # Cancellation must work over D-Bus while a file dialog is open.
        answers, errors = response_call(impl.OpenFile, handle, "", "", "LunaDash cancel test", options)
        client_id = wait_for(lambda: find_client("LunaDash cancel test"),
                             "Cancel picker missing")
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
        backend = launch("backend-cold-frontend", [str(BACKEND)],
                         environment=wayland_env)
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
        client_id = wait_for(lambda: find_client("LunaDash frontend test") or replies.get(path),
                             "Frontend did not invoke the LunaDash backend")
        assert path not in replies, replies
        enter_manual_path(client_id, str(fixture))
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
        chooser_evidence = BUILD / "ci-evidence" / "portal-screencast-chooser.png"
        chooser_evidence.parent.mkdir(parents=True, exist_ok=True)
        for index, (raw, accept) in enumerate((
            ("Monitor: HEADLESS-1 Display with trailing space  ", True),
            ("Window: editor \u5de5\u4f5c\u5340 (id-1)  ", True),
            ("Monitor: HEADLESS-1 Display with trailing space  ", False),
        )):
            chooser_log = (LOGDIR / f"chooser-{index}.log").open("w+")
            logs.append(chooser_log)
            chooser = subprocess.Popen([str(BACKEND), "--screencast-chooser"],
                                       env=wayland_env, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=chooser_log, text=True)
            processes.append(chooser)
            chooser.stdin.write(raw + "\n")
            chooser.stdin.close()
            def chooser_client():
                state = current_state()
                if not state:
                    return None
                return next((client["id"] for client in state.get("clients", [])
                             if client.get("mapped") and
                             client.get("title") == "Share your screen"), None)

            client_id = wait_for(chooser_client,
                                 "Slint ScreenCast chooser did not map as a Wayland client")
            control_request("focus", client_id)
            if index == 0:
                subprocess.run(["grim", str(chooser_evidence)], env=wayland_env,
                               check=True, timeout=12)
            subprocess.run(["wtype", "-k", "Return" if accept else "Escape"],
                           env=wayland_env, check=True, timeout=5)
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
