"""Exercise every option on every settings page through the control socket.

The settings UI is a thin layer over the control methods the compositor exposes,
so this test drives the same methods with the same payload shapes the pages send.
Every page is opened, every preference the pages can change is round-tripped with
one accepted and several rejected values, and the QML log must stay free of
binding errors. Icon resolution, GPU capability detection and the security review
are out of scope here and run in their own jobs.
"""

import json
import os
import re
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from PIL import Image

build = Path(sys.argv[1]).resolve()
repo = build.parent

# Every page file under qml/settings/pages, and the ids open-settings accepts.
PAGES = (
    "general",
    "modules",
    "windows",
    "shortcuts",
    "display",
    "input",
    "sound",
    "network",
    "bluetooth",
    "power",
    "applications",
    "privacy",
    "system",
    "devices",
    "about",
    "appearance",
)

# The tool catalogue categories the settings pages render with ToolList.
TOOL_CATEGORIES = (
    "display",
    "input",
    "sound",
    "network",
    "bluetooth",
    "privacy",
    "system",
    "devices",
    "applications",
)

APPEARANCE_DEFAULTS = {
    "accent": "#9ccbfb",
    "gap": 12,
    "panelHeight": 40,
    "blur": True,
    "blurRadius": 18,
    "windowOpacity": 96,
    "animations": True,
    "animationDuration": 220,
    "workspaceCount": 4,
    "masterRatio": 50,
    "defaultFloating": False,
    "keyboardLayout": "us",
    "keyRepeatRate": 25,
    "keyRepeatDelay": 600,
    "cursorSize": 24,
    "fontFamily": "sans-serif",
    "clock24Hour": True,
    "startupApps": [],
    "overview": False,
    "showHostDetails": False,
}

# (preference, accepted value, rejected values) for every desktop preference a
# settings page can change: AppearanceControls, EffectsControls, PreferenceSlider,
# the language, typography and clock controls, the startup switches, the windows
# page and the privacy page.
PREFERENCE_CASES = (
    ("accent", "#c4b5fd", ["#12345", "purple", 12, "#c4b5fdz", "#c4b5f", "c4b5fd"]),
    ("gap", 20, [3, 33, 8.5, "20", -1]),
    ("panelHeight", 36, [31, 57, 36.5, None]),
    ("blur", False, [1, "false", 0]),
    ("blurRadius", 24, [-1, 33, 12.5]),
    ("windowOpacity", 80, [59, 101, 80.5]),
    ("animations", False, ["no", 1, 0]),
    ("animationDuration", 260, [-1, 601, 260.5]),
    ("workspaceCount", 6, [0, 10, 3.5]),
    ("masterRatio", 60, [29, 71, 60.5]),
    ("defaultFloating", True, ["true", 1, 0]),
    ("keyboardLayout", "gb", ["xx", "GB", 5, ""]),
    ("keyRepeatRate", 30, [61, -1, 30.5]),
    ("keyRepeatDelay", 400, [199, 1501, 400.5]),
    ("cursorSize", 32, [15, 65, 32.5]),
    ("fontFamily", "monospace", ["comic-sans", "Monospace", 3]),
    ("clock24Hour", False, ["24", 1, 0]),
    (
        "startupApps",
        ["console"],
        [["nope"], ["console", "console"], ["console", 5], "console"],
    ),
    ("overview", True, [1, "true", 0]),
    ("showHostDetails", True, ["yes", 1, 0]),
)

SHORTCUT_DEFAULTS = {
    "focusLeft": "Meta+H",
    "focusRight": "Meta+L",
    "focusUp": "Meta+K",
    "focusDown": "Meta+J",
    "groupLeft": "Meta+Shift+H",
    "groupRight": "Meta+Shift+L",
    "reorderLeft": "Meta+Ctrl+H",
    "reorderRight": "Meta+Ctrl+L",
    "expelWindow": "Meta+Shift+E",
    "centerColumn": "Meta+Shift+C",
    "widenColumn": "Meta+=",
    "narrowColumn": "Meta+-",
    "maximizeWindow": "Meta+F",
    "closeWindow": "Meta+C",
    "minimizeWindow": "Meta+M",
    "closeWindowAlternate": "Meta+Q",
    "toggleFloating": "Meta+Space",
    "launchTerminal": "Meta+Return",
    "launchFiles": "Meta+E",
    "launchLauncher": "Meta+D",
    "screenshot": "Alt+Shift+F5",
}
for _workspace in range(1, 10):
    SHORTCUT_DEFAULTS["workspace%d" % _workspace] = "Meta+%d" % _workspace
    SHORTCUT_DEFAULTS["moveToWorkspace%d" % _workspace] = "Meta+Shift+%d" % _workspace

QML_FAILURES = (
    "ReferenceError",
    "TypeError",
    "SyntaxError",
    "Error loading QML",
    "is not a type",
    "Cannot assign to non-existent",
    "Cannot read property",
    "Unable to assign",
)


def page_files():
    return sorted(path.stem for path in (repo / "qml/settings/pages").glob("*.qml"))


def declared_control_methods():
    source = (repo / "src/compositor/wayland/WaylandCompositor.cpp").read_text(encoding="utf-8")
    return set(re.findall(r'method == "([a-z0-9-]+)"', source))


def used_control_methods():
    methods = set()
    for path in (repo / "qml").rglob("*.qml"):
        methods.update(
            re.findall(r'\.command\("([a-z0-9-]+)"', path.read_text(encoding="utf-8"))
        )
    return methods


assert page_files() == sorted(PAGES), (
    "The open-settings page list and qml/settings/pages disagree: " + str(page_files())
)
unknown_methods = used_control_methods() - declared_control_methods()
assert not unknown_methods, (
    "The shell calls control methods the compositor does not implement: "
    + ", ".join(sorted(unknown_methods))
)

with tempfile.TemporaryDirectory(prefix="ludash-settings-") as runtime:
    env = os.environ | {
        "XDG_RUNTIME_DIR": runtime,
        "XDG_CONFIG_HOME": runtime,
        "QT_QPA_PLATFORM": "xcb",
        "QT_XCB_GL_INTEGRATION": "xcb_egl",
        "LIBGL_ALWAYS_SOFTWARE": "1",
        "LUDASH_TEST_SETTINGS": "1",
        "LUDASH_SKIP_SETUP": "1",
        "LUDASH_LANGUAGE": "en_US",
        "QT_FORCE_STDERR_LOGGING": "1",
    }
    for key in ("MESA_GL_VERSION_OVERRIDE", "MESA_GLSL_VERSION_OVERRIDE"):
        env.pop(key, None)
    control = runtime + "/ludash-settings-control"

    def request(method="status", value=""):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(5)
            connection.connect(control)
            connection.sendall(
                json.dumps({"method": method, "value": str(value)}).encode() + b"\n"
            )
            result = b""
            while b"\n" not in result:
                chunk = connection.recv(65536)
                if not chunk or len(result) > 1024 * 1024:
                    raise RuntimeError("Invalid IPC response")
                result += chunk
            return json.loads(result)

    def apply(method, payload=None):
        encoded = "" if payload is None else json.dumps(payload)
        result = request(method, encoded)
        assert "error" not in result, (method, payload, result)
        return result

    def rejected(method, payload=None):
        encoded = "" if payload is None else json.dumps(payload)
        result = request(method, encoded)
        assert result.get("error"), ("Expected a rejection", method, payload, result)
        return result["error"]

    def wait_for(predicate, description, timeout=8):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                state = request()
                if predicate(state):
                    return state
            except (OSError, ValueError):
                pass
            time.sleep(0.1)
        raise AssertionError(description)

    with open(build / "settings.log", "w+") as log:
        process = subprocess.Popen(
            [
                str(build / "ludash-compositor"),
                "--socket",
                "ludash-settings",
                "--exit-after",
                "52000",
                "--screenshot",
                str(build / "settings-preview.png"),
            ],
            env=env,
            stdout=log,
            stderr=log,
        )
        try:
            state = wait_for(
                lambda data: data["layerSurfaces"] >= 2, "Shell did not map"
            )
            # The startup logo splash is a layer surface of its own, so waiting for
            # the plain desktop prevents "at least three surfaces" from being
            # satisfied by the splash instead of the settings surface.
            time.sleep(3.5)
            wait_for(lambda data: data["layerSurfaces"] == 2, "Desktop stayed covered")

            for key, value in (
                ("defaultApps", ("terminal", "files")),
                ("appearance", tuple(APPEARANCE_DEFAULTS)),
                ("shortcuts", tuple(SHORTCUT_DEFAULTS)),
                ("audio", ("installed", "output", "input", "busy")),
                ("power", ("installed", "available", "busy", "error")),
                ("sessionActions", ("suspend", "reboot", "poweroff")),
                ("update", ("status", "currentVersion", "repositoryUrl")),
                ("input", ("layout", "repeatRate", "repeatDelay")),
                (
                    "display",
                    ("width", "height", "scale", "output", "nested", "fullscreen"),
                ),
                ("system", ("os", "kernel", "architecture")),
                ("network", ("label",)),
                ("tiling", ("focusedWindow", "columns", "groups")),
            ):
                assert key in state, "Missing state section: " + key
                for field in value:
                    assert field in state[key], (key, field, state[key])
            assert set(state["appearance"]) == set(APPEARANCE_DEFAULTS), state[
                "appearance"
            ]
            assert set(state["shortcuts"]) == set(SHORTCUT_DEFAULTS), (
                "Shortcut set changed"
            )
            assert state["language"] == "en_US", state["language"]

            # Every settings page must open, report itself and map the surface.
            first_serial = state["settingsSerial"]
            for page in PAGES:
                result = request("open-settings", page)
                assert "error" not in result, (page, result)
                assert result["settingsPage"] == page, (page, result["settingsPage"])
                wait_for(lambda data: data["layerSurfaces"] >= 3, page + " did not map")
                time.sleep(0.9)
            for bad in ("nope", "../../shell", "General", ""):
                if not bad:
                    continue
                rejected("open-settings", bad)
            assert request()["settingsSerial"] != first_serial, "No page reload"
            assert request()["settingsPage"] == PAGES[-1], "Last page did not stick"

            # Every desktop preference: accepted, then rejected, then unchanged.
            for key, good, bad_values in PREFERENCE_CASES:
                apply("appearance", {key: good})
                wait_for(
                    lambda data, key=key, good=good: data["appearance"][key] == good,
                    "%s did not apply" % key,
                )
                for bad in bad_values:
                    rejected("appearance", {key: bad})
                    assert request()["appearance"][key] == good, (key, bad)
            assert request()["appearance"]["panelHeight"] == 36
            assert request()["panelExtent"] == 36, (
                "Panel height did not follow the preference"
            )
            rejected("appearance", {"nope": 1})
            assert request()["appearance"]["accent"] == "#c4b5fd"

            # Shortcut recorder: every action accepts and stores a new binding.
            for key in SHORTCUT_DEFAULTS:
                apply("shortcuts", {key: "Disabled"})
                assert request()["shortcuts"][key] == "Disabled", key
            apply("reset-shortcuts")
            assert request()["shortcuts"] == SHORTCUT_DEFAULTS, (
                "Shortcut reset changed defaults"
            )
            apply("shortcuts", {"focusLeft": "Meta+U"})
            assert request()["shortcuts"]["focusLeft"] == "Meta+U"
            for bad in (
                {"focusLeft": "Ctrl+H"},
                {"focusLeft": "Shift+H"},
                {"focusLeft": "Meta"},
                {"focusLeft": "Meta+L"},
                {"focusLeft": "Meta+U, Meta+Y"},
                {"focusLeft": 5},
                {"focusLeft": None},
                {"nope": "Meta+U"},
            ):
                rejected("shortcuts", bad)
            assert request()["shortcuts"]["focusLeft"] == "Meta+U"
            apply("reset-shortcuts")
            assert request()["shortcuts"] == SHORTCUT_DEFAULTS, (
                "Shortcut reset changed defaults"
            )

            # Language selection stays inside the shipped language pack.
            for language in ("zh_TW", "en_US"):
                apply("language", language)
                assert request()["language"] == language
            apply("language", "zh_TW")
            assert request()["translations"], (
                "Traditional Chinese language pack is empty"
            )
            for bad in ("fr_FR", "en", "zh-tw"):
                rejected("language", bad)
            assert request()["language"] == "en_US"

            # Workspace buttons and the bound check.
            assert request()["appearance"]["workspaceCount"] == 6
            for workspace in range(6):
                apply("workspace", workspace)
                assert request()["workspace"] == workspace
            for bad in ("-1", "6", "99", "abc", "1.5"):
                rejected("workspace", bad)
            apply("appearance", {"workspaceCount": 2})
            assert request()["workspace"] == 1, (
                "Removed workspace stranded the active desktop"
            )
            apply("workspace", 0)

            # Sound: the wire shape is validated even where WirePlumber is absent.
            for bad in (
                {"device": "output"},
                {"device": "output", "volume": 101},
                {"device": "output", "volume": -1},
                {"device": "output", "volume": 50.5},
                {"device": "output", "volume": "50"},
                {"device": "output", "volume": 50, "mute": True},
                {"device": "speaker", "volume": 50},
                {"device": "input", "mute": "yes"},
                {},
                [1, 2],
            ):
                rejected("audio", bad)
            if state["audio"]["installed"]:
                apply("audio", {"device": "output", "volume": 70})
                apply("audio", {"device": "output", "mute": False})

            # Power profiles: only profiles the service advertises are accepted.
            for bad in ("arbitrary-profile", "", "Balanced"):
                rejected("power-profile", bad)
            if state["power"].get("profiles"):
                apply("power-profile", state["power"]["profiles"][0])

            # Session actions are validated; a real action is never executed here
            # because it would reboot the machine running the tests.
            for bad in ("hibernate", "", "shutdown", "Reboot"):
                rejected("session-action", bad)
            assert set(state["sessionActions"]) >= {"suspend", "reboot", "poweroff"}

            # The settings tool catalogue backs every ToolList on every page.
            tools = state["settingsTools"]
            assert tools, "No system settings tools were reported"
            assert {tool["category"] for tool in tools} >= set(TOOL_CATEGORIES), state[
                "settingsTools"
            ]
            for tool in tools:
                assert set(tool) == {
                    "id",
                    "category",
                    "name",
                    "package",
                    "available",
                    "host",
                }, tool
                if not tool["available"]:
                    rejected("system-tool", tool["id"])
            rejected("system-tool", "/bin/sh")
            rejected("system-tool", "nope")

            # Default applications and startup selection.
            apply("default-apps", {"terminal": []})
            apply("default-apps", {"files": []})
            apply("default-apps", {"files": ["cat"]})
            assert request()["defaultApps"]["files"] == ["cat"]
            for bad in (
                {"files": ["ludash-desktop"]},
                {"files": ["-oops"]},
                {"files": ["/nonexistent/ludash-missing-app"]},
                {"files": "cat"},
                {"files": [1]},
                {"files": ["cat"] * 25},
                {"browser": []},
                {"terminal": [], "files": {"program": "cat"}},
            ):
                rejected("default-apps", bad)
            assert request()["defaultApps"]["files"] == ["cat"]
            apply("default-apps", {"terminal": [], "files": []})
            assert request()["defaultApps"] == {"terminal": [], "files": []}
            rejected("launch-default", "browser")

            # Wallpapers: shader palettes, an explicit image and the default.
            for palette in (0, 1):
                apply("wallpaper", palette)
                assert request()["wallpaper"] == palette
                assert not request()["wallpaperImage"], (
                    "Shader palette kept an image wallpaper"
                )
            rejected("wallpaper", 2)
            rejected("wallpaper-image", runtime + "/missing-ludash-wallpaper.png")
            image = Path(runtime) / "wallpaper.png"
            Image.new("RGB", (16, 10), (24, 32, 48)).save(image)
            apply("wallpaper-image", str(image))
            assert request()["wallpaperImage"].endswith("wallpaper.png")
            apply("wallpaper-default")
            assert request()["wallpaperImage"], "Default wallpaper is missing"
            before_picker = request()["pickerSerial"]
            apply("choose-wallpaper")
            state = request()
            assert state["pickerSerial"] != before_picker, (
                "Wallpaper picker did not open"
            )
            assert state["settingsPage"] == "appearance", state["settingsPage"]
            rejected("choose-wallpaper", "value")

            # Shell modules: validation, saving, trust and restoring built-ins.
            apply("module-validate", request()["shellModules"]["document"])
            rejected("module-validate", "{")
            assert not state["shellModules"]["trusted"], "Custom code started trusted"
            apply(
                "module-save",
                {
                    "schemaVersion": 1,
                    "modules": {
                        "panel": {
                            "style": {
                                "height": 48,
                                "margin": 8,
                                "radius": 22,
                                "edge": "bottom",
                            }
                        }
                    },
                },
            )
            state = request()
            assert state["panelExtent"] == 64, state["panelExtent"]
            assert state["panelAtBottom"] is True, state["panelAtBottom"]
            rejected(
                "module-save",
                {"schemaVersion": 1, "modules": {"settings": {"enabled": False}}},
            )
            assert request()["panelExtent"] == 64, "Rejected save changed the panel"
            apply("module-code-trust", "true")
            assert request()["shellModules"]["trusted"] is True
            apply("module-code-trust", "false")
            assert request()["shellModules"]["trusted"] is False
            rejected("module-code-trust", "maybe")
            apply("module-reset")
            state = request()
            assert state["panelExtent"] == 40 and state["panelAtBottom"] is False, (
                state["panelExtent"]
            )
            assert not state["shellModules"]["trusted"]
            apply("module-template", "panel")
            rejected("module-template", "bogus")
            apply("module-reset")

            # Display presets: the invalid sizes are refused, and the running size
            # is accepted without changing the desktop.
            for bad in ("0x0", "abc", "-1x-1", "9999x9999", "1440x", ""):
                rejected("desktop-size", bad)
            display = request()["display"]
            current_size = "%dx%d" % (display["width"], display["height"])
            if (
                display["nested"]
                and not display["fullscreen"]
                and current_size == "1440x900"
            ):
                apply("desktop-size", current_size)

            # Remaining control surfaces the shell uses.
            apply("shortcut-capture", "true")
            apply("shortcut-capture", "false")
            rejected("shortcut-capture", "maybe")
            # Copy, cut, paste and select-all act on the focused application, so
            # without one the compositor reports that no window has keyboard focus.
            for key in ("copy", "paste", "cut", "selectAll"):
                result = request("send-key", key)
                assert "error" not in result or "keyboard focus" in result["error"], (
                    result
                )
            rejected("send-key", "bogus")
            for bad in (
                '{"window":1,"target":2}',
                "nope",
                "",
                '{"window":0,"target":0}',
            ):
                rejected("group-window", bad)
            for bad in ("999", "abc", "0", "-1"):
                rejected("expel-window", bad)
            for method in ("focus", "minimize", "close"):
                for bad in ("999", "abc", "0"):
                    rejected(method, bad)
            apply("setup")
            apply("finish-setup")
            assert request()["setupComplete"] is True
            apply("check-update")
            assert request()["update"]["status"] in {
                "checking",
                "upToDate",
                "available",
                "error",
            }, request()["update"]

            # Restoring defaults must clear every preference the pages own.
            apply("reset-preferences")
            state = request()
            assert state["appearance"] == APPEARANCE_DEFAULTS, state["appearance"]
            assert state["shortcuts"] == SHORTCUT_DEFAULTS
            assert state["defaultApps"] == {"terminal": [], "files": []}
            assert state["workspace"] == 0

            assert process.wait(timeout=58) == 0, (
                "Settings session did not close cleanly"
            )

            log.flush()
            log.seek(0)
            output = log.read()
            for page in PAGES:
                assert "Settings page loaded: " + page in output, (
                    "Settings page was not loaded: " + page
                )
            for failure in QML_FAILURES:
                assert failure not in output, failure + "\n" + output[-8000:]
            print(
                "Settings passed: 16 pages, "
                + str(len(PREFERENCE_CASES))
                + " appearance preferences, "
                + str(len(SHORTCUT_DEFAULTS))
                + " shortcuts, language, workspaces, audio, power, session actions, "
                + str(len(tools))
                + " system tools, default apps, wallpapers, modules, display and reset."
            )
        except BaseException:
            log.flush()
            log.seek(0)
            print(log.read()[-12000:], file=sys.stderr)
            raise
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
