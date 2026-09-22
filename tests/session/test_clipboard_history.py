#!/usr/bin/env python3
import json
import os
import pathlib
import subprocess
import sys
import tempfile

root = pathlib.Path(__file__).resolve().parents[2]
helper = root / "scripts/lunadash-clipboard-history"

with tempfile.TemporaryDirectory(prefix="lunadash-clipboard-test-") as runtime:
    bin_dir = pathlib.Path(runtime) / "bin"
    bin_dir.mkdir()
    copied = pathlib.Path(runtime) / "copied.bin"

    wl_copy = bin_dir / "wl-copy"
    wl_copy.write_text(
        "#!/bin/sh\ncat > \"$TEST_CLIPBOARD_COPY\"\n",
        encoding="utf-8",
    )
    wl_copy.chmod(0o755)

    env = os.environ | {
        "XDG_RUNTIME_DIR": runtime,
        "PATH": str(bin_dir) + os.pathsep + os.environ.get("PATH", ""),
        "TEST_CLIPBOARD_COPY": str(copied),
    }

    def run(*args, data=None):
        return subprocess.run(
            [str(helper), *args],
            input=data,
            env=env,
            capture_output=True,
            check=False,
        )

    assert run("record", data="first\nline".encode()).returncode == 0
    assert run("record", data="second".encode()).returncode == 0
    assert run("record", data="first\nline".encode()).returncode == 0
    listed = run("list")
    assert listed.returncode == 0, listed.stderr
    entries = json.loads(listed.stdout)
    assert [entry["text"] for entry in entries] == ["first\nline", "second"]

    assert run("copy", "1").returncode == 0
    assert copied.read_bytes() == b"second"
    entries = json.loads(run("list").stdout)
    assert [entry["text"] for entry in entries] == ["second", "first\nline"]

    history = pathlib.Path(runtime) / "lunadash/clipboard-history.json"
    assert history.stat().st_mode & 0o077 == 0

    assert run("clear").returncode == 0
    assert json.loads(run("list").stdout) == []

print("Clipboard history passed: deduplication, multiline text, copy, clear and owner-only storage.")
