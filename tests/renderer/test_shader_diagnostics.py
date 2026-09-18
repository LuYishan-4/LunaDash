"""Ensure modular renderer failures cannot produce a green result."""
from pathlib import Path
import subprocess
import sys
import tempfile

checker = Path(__file__).resolve().parents[2] / "scripts/testing/check_graphics_log.py"
with tempfile.TemporaryDirectory(prefix="ludash-render-log-") as directory:
    log = Path(directory) / "graphics.log"
    for text, expected in (
        ("LunaDash compositor backend: wlroots\n", 0),
        ("Wallpaper renderer initialization failed: shader compile error\n", 1),
        ("Backdrop blur shader failed: invalid stage\n", 1),
        ("wlroots could not create a renderer\n", 1),
        ("wlroots scene output commit failed\n", 1),
        ("No GLSL shader code found\n", 1),
        ("Failed to build graphics pipeline state\n", 1),
        ("Could not create EGL surface\n", 1),
        ("eglSwapBuffers failed\n", 1),
        ("QProcess: Cannot create pipe (Too many open files)\n", 1),
    ):
        log.write_text(text)
        result = subprocess.run(
            [sys.executable, str(checker), str(log)], capture_output=True
        )
        assert result.returncode == expected, result.stderr.decode()
print("Graphics diagnostic checks passed for wlroots and modular render paths.")
