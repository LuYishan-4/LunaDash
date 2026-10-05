"""Renderer startup errors must exit with a diagnostic instead of aborting."""
import os
import pathlib
import re
import subprocess
import sys
import tempfile

binary = str(pathlib.Path(sys.argv[1]).resolve())
os.environ["QT_FORCE_STDERR_LOGGING"] = "1"

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
    assert re.search(r"LunaDash renderer: (gles2|pixman|vulkan)", result.stderr), result.stderr
print("Renderer startup handling validated invalid CLI input and wlroots fallback.")

with tempfile.TemporaryDirectory(prefix="ludash-vulkan-failure-") as runtime:
    os.chmod(runtime, 0o700)
    env = os.environ | {
        "XDG_RUNTIME_DIR": runtime, "XDG_CONFIG_HOME": runtime,
        "WLR_BACKENDS": "headless", "WLR_RENDERER": "vulkan",
        "VK_DRIVER_FILES": runtime + "/missing-icd.json",
        "VK_ICD_FILENAMES": runtime + "/missing-icd.json",
        "LUDASH_DISABLE_XWAYLAND": "1",
    }
    result = subprocess.run([binary, "--graphics", "vulkan", "--no-shell"],
        env=env, capture_output=True, text=True, timeout=15)
    assert result.returncode == 2, (result.returncode, result.stderr)
    assert "Vulkan" in result.stderr, result.stderr
    assert "LunaDash renderer: pixman" not in result.stderr, result.stderr
print("Explicit Vulkan failure is diagnosed without silently selecting pixman.")
