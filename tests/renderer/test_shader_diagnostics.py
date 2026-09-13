"""Ensure a missing Qt shader cannot produce a green integration result."""
from pathlib import Path
import subprocess
import sys
import tempfile

checker = Path(__file__).resolve().parents[2] / "scripts/testing/check_graphics_log.py"
with tempfile.TemporaryDirectory(prefix="ludash-shader-log-") as directory:
    log = Path(directory) / "graphics.log"
    for text, expected in (
        ("LuDash graphics context: OpenGL 3 . 3 (compatibility profile)\n", 0),
        ("No GLSL shader code found (versions tried: QList(330, 150, 140, 130))\n", 1),
        ("Failed to build graphics pipeline state\n", 1),
        ("EglClientBufferIntegration: creating texture with no current context\n", 1),
        ("No QSGTexture provided from updateSampledImage(). This is wrong.\n", 1),
        ("Could not create EGL surface (EGL error 0x321c)\n", 1),
        ("eglSwapBuffers failed with 0x300d\n", 1),
    ):
        log.write_text(text)
        result = subprocess.run([sys.executable, str(checker), str(log)], capture_output=True)
        assert result.returncode == expected, result.stderr.decode()
print("Graphics diagnostic checks passed: missing shader and pipeline failures rejected.")
