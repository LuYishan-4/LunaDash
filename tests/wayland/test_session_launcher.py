"""Exercise login-script boundaries without acquiring a GPU or changing services."""

import json
import os
import subprocess
import socket
import sys
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[2]
installer = root / "scripts/install-session.sh"
assert subprocess.run([str(installer), "--help"], capture_output=True).returncode == 0
assert (
    subprocess.run([str(installer), "--unknown"], capture_output=True).returncode == 2
)
with tempfile.TemporaryDirectory(prefix="ludash-login-test-") as directory:
    folder = Path(directory)
    binary = folder / "bin"
    binary.mkdir()
    defaults = folder / "share/xdg-desktop-portal"
    defaults.mkdir(parents=True)
    routing = (root / "data/portal/lunadash-portals.conf").read_text()
    (defaults / "lunadash-portals.conf").write_text(routing)
    user_config = folder / "config/xdg-desktop-portal"
    user_config.mkdir(parents=True)
    generic = user_config / "portals.conf"
    generic.write_text("[preferred]\ndefault=kde\n")
    desktop_config = user_config / "lunadash-portals.conf"

    def executable(name, content):
        target = binary / name
        target.write_text(content)
        target.chmod(0o755)

    executable("ludash-session", (root / "scripts/ludash-session").read_text())
    executable("id", "#!/bin/sh\necho 1000\n")
    for command in ("quickshell", "konsole", "fish", "pacman", "makepkg", "sudo"):
        executable(command, "#!/bin/sh\nexit 97\n")
    executable(
        "dbus-run-session",
        f"""#!{sys.executable}
import json, os, sys
from pathlib import Path
if len(sys.argv) < 3 or sys.argv[1] != '--':
    raise SystemExit(2)
Path(os.environ["TEST_DBUS_RECORD"]).write_text(json.dumps(dict(os.environ)))
os.execvp(sys.argv[2], sys.argv[2:])
""",
    )
    executable(
        "ludash-compositor",
        f'#!{sys.executable}\nimport json, os, sys\nfrom pathlib import Path\nPath(os.environ["TEST_RECORD"]).write_text(json.dumps({{"args": sys.argv[1:], "env": dict(os.environ)}}))\n',
    )
    record = folder / "launch.json"
    dbus_record = folder / "dbus.json"
    env = os.environ | {
        "PATH": str(binary) + os.pathsep + os.environ["PATH"],
        "XDG_RUNTIME_DIR": directory,
        "XDG_CONFIG_HOME": str(folder / "config"),
        "XDG_STATE_HOME": str(folder / "state"),
        "TEST_RECORD": str(record),
        "TEST_DBUS_RECORD": str(dbus_record),
        "DBUS_SESSION_BUS_ADDRESS": f"unix:path={folder / 'user-bus'}",
        "DISPLAY": ":999",
        "WAYLAND_DISPLAY": "host",
        "LIBGL_ALWAYS_SOFTWARE": "1",
        "MESA_GL_VERSION_OVERRIDE": "2.1",
        "MESA_GLSL_VERSION_OVERRIDE": "120",
        "QT_QUICK_BACKEND": "software",
    }
    for key in ("LUDASH_GRAPHICS", "QT_QPA_EGLFS_INTEGRATION"):
        env.pop(key, None)
    launcher = str(binary / "ludash-session")

    def run(*args, environment=env):
        return subprocess.run(
            [launcher, *args], env=environment, capture_output=True, text=True
        )

    assert run("--check").returncode == 0
    assert not record.exists(), "Preflight launched the compositor"
    assert not desktop_config.exists(), "Preflight changed portal routing"
    assert run(environment=env | {"XDG_RUNTIME_DIR": ""}).returncode != 0
    assert run(environment=env | {"LUDASH_GRAPHICS": "invalid"}).returncode == 2
    literal = "socket;echo injected"
    result = run("--socket", literal)
    assert result.returncode == 0, result.stderr
    launch = json.loads(record.read_text())
    assert launch["args"] == ["--fullscreen", "--graphics", "gles", "--socket", literal]
    assert launch["env"]["QT_QPA_PLATFORM"] == "eglfs"
    assert launch["env"]["QT_QPA_EGLFS_INTEGRATION"] == "eglfs_kms"
    assert launch["env"]["XDG_CURRENT_DESKTOP"] == "LunaDash"
    assert launch["env"]["XDG_SESSION_DESKTOP"] == "LunaDash"
    assert launch["env"]["XMODIFIERS"] == "@im=fcitx"
    assert launch["env"]["LUNADASH_PUBLISH_ACTIVATION_ENV"] == "1"
    assert launch["env"]["QT_IM_MODULE"] == "fcitx"
    assert launch["env"]["QT_IM_MODULES"] == "wayland;fcitx;ibus"
    assert launch["env"]["GTK_IM_MODULE"] == "fcitx"
    assert launch["env"]["SDL_IM_MODULE"] == "fcitx"
    assert launch["env"]["DBUS_SESSION_BUS_ADDRESS"] == env["DBUS_SESSION_BUS_ADDRESS"]
    assert not dbus_record.exists(), "Existing user D-Bus was replaced"
    assert desktop_config.read_text() == routing
    assert generic.read_text() == "[preferred]\ndefault=kde\n"
    # Explicit LunaDash overrides are user-owned; later logins must preserve them.
    desktop_config.write_text("[preferred]\ndefault=gtk\n# custom routing\n")

    fallback_record = folder / "fallback-launch.json"
    fallback_env = dict(env)
    fallback_env.pop("DBUS_SESSION_BUS_ADDRESS", None)
    fallback_env["TEST_RECORD"] = str(fallback_record)
    result = run("--socket", "fallback", environment=fallback_env)
    assert result.returncode == 0, result.stderr
    fallback_launch = json.loads(fallback_record.read_text())
    assert fallback_launch["args"] == [
        "--fullscreen", "--graphics", "gles", "--socket", "fallback"
    ]
    assert desktop_config.read_text() == "[preferred]\ndefault=gtk\n# custom routing\n"
    dbus_env = json.loads(dbus_record.read_text())
    for name in ("QT_QPA_PLATFORM", "QT_QPA_EGLFS_INTEGRATION"):
        assert name not in dbus_env, f"{name} leaked into D-Bus daemon environment"
    for name in (
        "DISPLAY",
        "WAYLAND_DISPLAY",
        "LIBGL_ALWAYS_SOFTWARE",
        "MESA_GL_VERSION_OVERRIDE",
        "MESA_GLSL_VERSION_OVERRIDE",
        "QT_QUICK_BACKEND",
    ):
        assert name not in launch["env"], name
    logs = list((folder / "state/lunadash").glob("session-*.log"))
    assert len(logs) == 2
    # An available PAM user bus wins over spawning a private dbus-run-session.
    dbus_record.unlink()
    with socket.socket(socket.AF_UNIX) as user_bus:
        user_bus.bind(str(folder / "bus"))
        result = run("--socket", "pam-bus", environment=fallback_env)
        assert result.returncode == 0, result.stderr
        reused = json.loads(fallback_record.read_text())
        assert reused["env"]["DBUS_SESSION_BUS_ADDRESS"] == f"unix:path={folder / 'bus'}"
        assert not dbus_record.exists(), "PAM user bus was ignored"
    assert all(log.stat().st_mode & 0o777 == 0o600 for log in logs)
    result = subprocess.run(
        [str(installer), "--dry-run"], env=env, capture_output=True, text=True
    )
    if os.geteuid() == 0:
        assert result.returncode == 1 and "normal user" in result.stderr
    else:
        assert result.returncode == 0, result.stderr
        assert "makepkg --syncdeps --force --install" in result.stdout
        assert "systemctl enable" not in result.stdout
        assert "97" not in result.stdout
        # Emulate a machine without an enabled display manager. No fake tool
        # should execute: all boot configuration remains a dry-run plan.
        executable("readlink", "#!/bin/sh\nexit 0\n")
        executable("systemctl", "#!/bin/sh\nexit 97\n")
        result = subprocess.run(
            [str(installer), "--enable-sddm", "--dry-run"],
            env=env,
            capture_output=True,
            text=True,
        )
        assert result.returncode == 0, result.stderr
        assert "systemctl enable sddm.service" in result.stdout
        assert "systemctl set-default graphical.target" in result.stdout
        assert "--now" not in result.stdout
print(
    "Login scripts passed: no side effects in preflight/dry-run, isolated environment, literal arguments and private logs."
)
