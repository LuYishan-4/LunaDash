"""Verify LunaDash ScreenCast uses a source list instead of slurp."""

import os
import subprocess
import sys
from pathlib import Path

build = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parents[2]
config = root / "data/portal/xdg-desktop-portal-wlr/LunaDash"
chooser = build / "lunadash-screencast-chooser"

text = config.read_text(encoding="utf-8")
assert "chooser_type=dmenu" in text, text
assert "chooser_cmd=lunadash-screencast-chooser" in text, text
assert "slurp" not in "\n".join(
    line for line in text.splitlines() if not line.lstrip().startswith("#")
), text

sources = "Monitor: HEADLESS-1 Headless output 1\nWindow: Discord (discord)\n"
result = subprocess.run(
    [str(chooser)],
    input=sources,
    text=True,
    capture_output=True,
    timeout=5,
    env=os.environ | {"LUNADASH_SCREENCAST_CHOOSER_AUTOPICK": "1"},
)
assert result.returncode == 0, result.stderr or result.stdout
assert result.stdout.strip() == "Monitor: HEADLESS-1 Headless output 1", result.stdout

print("ScreenCast chooser passed: list protocol is active and slurp is not used.")
