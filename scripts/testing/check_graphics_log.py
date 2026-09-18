"""Reject renderer failures even when the compositor exits successfully."""
from pathlib import Path
import sys

def failed_graphics_messages(text):
    return [message for message in (
        "No GLSL shader code found",
        "Failed to build graphics pipeline state",
        "Wallpaper renderer initialization failed",
        "Wallpaper render failed",
        "Backdrop blur shader failed",
        "Backdrop blur render failed",
        "could not resize blur framebuffers",
        "wlroots could not create a renderer",
        "wlroots scene output commit failed",
        "creating texture with no current context",
        "No QSGTexture provided from updateSampledImage",
        "Could not create EGL surface",
        "eglSwapBuffers failed",
        "Too many open files",
        "Failed to write to the pipe:",
        "QProcess: Cannot create pipe",
    ) if message in text]

if __name__ == "__main__":
    failures = failed_graphics_messages(
        Path(sys.argv[1]).read_text(errors="replace")
    )
    if failures:
        sys.exit("Graphics validation failed: " + "; ".join(failures))
