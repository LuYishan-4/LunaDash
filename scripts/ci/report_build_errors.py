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
CONTEXT_AFTER = 8
ANNOTATION_LIMIT = 9
ANNOTATION_BYTES = 60000


def escape(text: str) -> str:
    return text.replace("%", "%25").replace("\r", "").replace("\n", "%0A")


def collect(lines: list[str]) -> list[str]:
    blocks, seen, index = [], set(), 0
    while index < len(lines):
        line = lines[index]
        if ERROR.search(line) and "-Werror" not in line[:20]:
            block = lines[index:index + 1 + CONTEXT_AFTER]
            key = line.strip()
            if key not in seen:
                seen.add(key)
                blocks.append("\n".join(item[:400] for item in block))
            index += 1 + CONTEXT_AFTER
            continue
        index += 1
    return blocks


def main() -> int:
    if len(sys.argv) < 3:
        print("usage: report_build_errors.py <label> <log>...", file=sys.stderr)
        return 2
    label = sys.argv[1]
    lines = []
    for name in sys.argv[2:]:
        path = Path(name)
        if path.is_file():
            lines.append(f"==> {path.name}")
            lines.extend(path.read_text(encoding="utf-8", errors="replace").splitlines())
    if not lines:
        return 0
    blocks = collect(lines)
    if not blocks:
        tail = "\n".join(lines[-60:])
        print(f"::error title={label} build log tail::{escape(tail)}")
        return 0
    chunk, chunks = "", []
    for block in blocks:
        if len(chunk) + len(block) + 2 > ANNOTATION_BYTES:
            chunks.append(chunk)
            chunk = ""
        chunk += block + "\n\n"
    if chunk:
        chunks.append(chunk)
    for number, text in enumerate(chunks[:ANNOTATION_LIMIT], 1):
        print(f"::error title={label} build errors {number}/{len(chunks)}::{escape(text)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
