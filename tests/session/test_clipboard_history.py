#!/usr/bin/env python3
import json
import os
import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[2]
helper = root / "scripts/lunadash-clipboard-history"

with tempfile.TemporaryDirectory(prefix="lunadash-clipboard-test-") as runtime:
    bin_dir = pathlib.Path(runtime) / "bin"
    bin_dir.mkdir()
    copied = pathlib.Path(runtime) / "copied.bin"
    copied_type = pathlib.Path(runtime) / "copied.type"

    wl_copy = bin_dir / "wl-copy"
    wl_copy.write_text(
        "#!/bin/sh\n"
        "printf '%s' \"$2\" > \"$TEST_CLIPBOARD_TYPE\"\n"
        "cat > \"$TEST_CLIPBOARD_COPY\"\n",
        encoding="utf-8",
    )
    wl_copy.chmod(0o755)

    env = os.environ | {
        "XDG_RUNTIME_DIR": runtime,
        "PATH": str(bin_dir) + os.pathsep + os.environ.get("PATH", ""),
        "TEST_CLIPBOARD_COPY": str(copied),
        "TEST_CLIPBOARD_TYPE": str(copied_type),
    }

    def run(*args, data=None, extra_env=None):
        return subprocess.run(
            [str(helper), *args],
            input=data,
            env=env | (extra_env or {}),
            capture_output=True,
            check=False,
        )

    assert run("record", data=b"first\nline").returncode == 0
    assert run("record", data=b"second").returncode == 0
    assert run("record", data=b"first\nline").returncode == 0

    uri = b"file:///tmp/movie.mp4\nfile:///tmp/picture.png\n"
    assert run("record", "text/uri-list", data=uri).returncode == 0

    image = b"\x89PNG\r\n\x1a\n" + bytes(range(32))
    assert run("record", "image/png", data=image).returncode == 0

    audio = b"OggS" + bytes(range(64))
    assert run("record", "audio/ogg", data=audio).returncode == 0

    video = b"\x00\x00\x00\x18ftypmp42" + bytes(range(48))
    assert run("record", "video/mp4", data=video).returncode == 0

    entries = json.loads(run("list").stdout)
    assert [entry["kind"] for entry in entries[:4]] == [
        "video", "audio", "image", "files"
    ], entries
    assert entries[3]["mime"] == "text/uri-list"
    assert "movie.mp4" in entries[3]["preview"]
    assert entries[2]["payload"].startswith(
        str(pathlib.Path(runtime) / "lunadash/clipboard-payloads")
    )

    assert run("copy", "0").returncode == 0
    assert copied.read_bytes() == video
    assert copied_type.read_text() == "video/mp4"

    # watch-record uses wl-paste's CLIPBOARD_TYPE environment and must preserve
    # arbitrary binary data instead of treating it as text.
    binary = b"\x00\x01\x02\xffbinary"
    assert run(
        "watch-record",
        data=binary,
        extra_env={"CLIPBOARD_TYPE": "application/octet-stream",
                   "CLIPBOARD_STATE": "data"},
    ).returncode == 0
    entries = json.loads(run("list").stdout)
    assert entries[0]["kind"] == "binary"
    assert entries[0]["mime"] == "application/octet-stream"

    history = pathlib.Path(runtime) / "lunadash/clipboard-history.json"
    payload_dir = pathlib.Path(runtime) / "lunadash/clipboard-payloads"
    assert history.stat().st_mode & 0o077 == 0
    assert payload_dir.stat().st_mode & 0o077 == 0
    for path in payload_dir.iterdir():
        assert path.stat().st_mode & 0o077 == 0

    assert run("clear").returncode == 0
    assert json.loads(run("list").stdout) == []
    assert list(payload_dir.iterdir()) == []

print(
    "Clipboard history passed: text, URI files, images, audio, video, "
    "binary MIME, replay, deduplication and owner-only payload storage."
)
