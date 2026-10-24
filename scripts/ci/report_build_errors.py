#!/usr/bin/env python3
"""Publish compiler, linker and CMake failures from a build log as annotations.

GitHub keeps annotation text available through the checks API even when raw
job logs are not reachable, so every distinct failure is reported in a few
large annotations instead of only the first problem-matcher hits.
"""
from pathlib import Path
import re
import sys

ERROR = re.compile(
    r"(?:\berror\b|undefined reference|multiple definition|cannot find -l|"
    r"^CMake Error|^FAILED: |ld returned|FAIL!|\*\*\*Failed|\bFailed\b|"
    r"Assertion|AssertionError|Traceback|Timeout|Segmentation fault|"
    r"Subprocess aborted|Child aborted|Exception)",
    re.IGNORECASE,
)
NOISE = re.compile(r"WARNING \*\*|dbind-WARNING|RealtimeKit|^\s*$")
CONTEXT_AFTER = 3
ANNOTATION_LIMIT = 9
# GitHub truncates annotation messages to 4 KiB.
ANNOTATION_BYTES = 3900


def escape(text: str) -> str:
    return text.replace("%", "%25").replace("\r", "").replace("\n", "%0A")


def collect(lines: list[str]) -> list[str]:
    blocks, seen, index = [], set(), 0
    while index < len(lines):
        line = lines[index]
        if ERROR.search(line) and "-Werror" not in line[:20] and not NOISE.search(line):
            context = 18 if "Traceback" in line else CONTEXT_AFTER
            block = [item for item in lines[index:index + 1 + context]
                     if not NOISE.search(item)]
            key = line.strip()
            if key not in seen:
                seen.add(key)
                blocks.append("\n".join(item[:240] for item in block))
            index += 1 + context
            continue
        index += 1
    return blocks


def main() -> int:
    if len(sys.argv) < 3:
        print("usage: report_build_errors.py <label> <log>...", file=sys.stderr)
        return 2
    label = sys.argv[1]
    # One group of annotations per log keeps a noisy log from hiding others.
    budget = ANNOTATION_LIMIT
    for name in sys.argv[2:]:
        path = Path(name)
        if not path.is_file() or budget <= 0:
            continue
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        blocks = collect(lines)
        if not blocks:
            continue
        chunks, chunk = [], ""
        for block in blocks:
            if len(chunk) + len(block) + 2 > ANNOTATION_BYTES:
                chunks.append(chunk)
                chunk = ""
            chunk += block[:ANNOTATION_BYTES] + "\n\n"
        if chunk:
            chunks.append(chunk)
        shown = chunks[:min(3, budget)]
        budget -= len(shown)
        for number, text in enumerate(shown, 1):
            print(f"::error title={label} {path.name} {number}/{len(chunks)}::{escape(text)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
