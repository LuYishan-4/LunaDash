"""Reject Qt pipeline failures even when a compositor exits successfully."""
from pathlib import Path
import sys

def failed_graphics_messages(text):
    return [message for message in (
        "No GLSL shader code found",
        "Failed to build graphics pipeline state",
        "creating texture with no current context",
        "No QSGTexture provided from updateSampledImage",
        "Could not create EGL surface",
        "eglSwapBuffers failed",
    ) if message in text]

if __name__ == "__main__":
    failures = failed_graphics_messages(Path(sys.argv[1]).read_text(errors="replace"))
    if failures:
        sys.exit("Graphics validation failed: " + "; ".join(failures))
