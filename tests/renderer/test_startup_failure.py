"""Renderer startup errors must exit with a diagnostic instead of aborting."""
import os
import pathlib
import subprocess
import sys
import tempfile

binary = str(pathlib.Path(sys.argv[1]).resolve())
for arguments in (["--graphics", "invalid"], ["--graphics"]):
    result = subprocess.run(
        [binary, *arguments], capture_output=True, text=True, timeout=5
    )
    assert result.returncode == 2, result
    assert "--graphics must be" in result.stderr, result.stderr

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
    assert result.returncode != 0, (result.returncode, result.stderr)
    assert "renderer" in result.stderr.lower(), result.stderr
print("Renderer startup failures returned clean diagnostics without aborting.")
