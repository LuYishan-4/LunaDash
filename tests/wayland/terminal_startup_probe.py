"""CI terminal child: draw once and record its PTY size from process startup."""

import json
import os
from pathlib import Path
import sys
import time

if os.environ.get("CI", "").lower() != "true":
    raise SystemExit("This terminal probe runs only in CI.")

initial = os.get_terminal_size()
columns, rows = initial
print("\033[2J\033[H\033[38;2;150;195;245m", end="")
print("LunaDash - application startup layout")
print(f"First PTY size: {columns} columns x {rows} rows")
print()
for index in range(min(12, rows - 5)):
    label = f"ROW {index + 1:02d}"
    value = "content drawn at the initial configured size"
    print(label + " " * max(1, columns - len(label) - len(value) - 2) + value)
sys.stdout.flush()

samples = [list(initial)]
for _ in range(10):
    time.sleep(0.1)
    samples.append(list(os.get_terminal_size()))
Path(sys.argv[1]).write_text(json.dumps({"sizes": samples}, indent=2))
while True:
    time.sleep(30)
