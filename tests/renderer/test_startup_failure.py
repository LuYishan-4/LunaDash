"""Renderer startup errors must exit with a diagnostic instead of aborting."""
import os
import pathlib
import subprocess
import sys
import tempfile

binary = str(pathlib.Path(sys.argv[1]).resolve())

invalid = subprocess.run(
    [binary, "--graphics", "invalid"], capture_output=True, text=True, timeout=5
)
assert invalid.returncode == 2, invalid
assert "--graphics must be" in invalid.stderr, invalid.stderr

missing = subprocess.run(
    [binary, "--graphics"], capture_output=True, text=True, timeout=5
)
assert missing.returncode == 1, missing
assert "Missing value after '--graphics'" in missing.stderr, missing.stderr

with tempfile.TemporaryDirectory(prefix="ludash-render-failure-") as runtime:
    os.chmod(runtime, 0o700)
    env = os.environ | {
        "XDG_RUNTIME_DIR": runtime,
        "XDG_CONFIG_HOME": runtime,
        "WLR_BACKENDS": "headless",
        "WLR_RENDERER": "not-a-renderer",
        "LUDASH_DISABLE_XWAYLAND": "1",
    }
    result = subprocess.run(
        [binary, "--no-shell", "--exit-after", "1500"],
        env=env,
        capture_output=True,
        text=True,
        timeout=12,
    )
    assert result.returncode == 0, (result.returncode, result.stderr)
    stderr = result.stderr.lower()
    assert "unknown wlr_renderer option" in stderr, result.stderr
    assert "pixman renderer" in stderr, result.stderr
print("Renderer startup handling validated invalid CLI input and wlroots fallback.")
