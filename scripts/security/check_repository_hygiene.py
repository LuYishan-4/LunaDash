"""Reject credentials, personal filesystem paths, and unsafe workflow patterns."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
SOURCE_SUFFIXES = {
    ".c", ".cc", ".cpp", ".h", ".hh", ".hpp", ".qml", ".py", ".sh",
    ".txt", ".cmake", ".yml", ".yaml", ".json",
}
SECRET_PATTERNS = (
    r"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----",
    r"\b(?:AKIA|ASIA)[0-9A-Z]{16}\b",
    r"\bgh[pousr]_[A-Za-z0-9_]{20,}\b",
    r"\bgithub_pat_[A-Za-z0-9_]{20,}\b",
    r"\bxox[baprs]-[A-Za-z0-9-]{20,}\b",
    r"(?i)\b(?:api[_ -]?key|secret[_ -]?key|access[_ -]?token)\s*[:=]\s*['\"][^'\"]{12,}['\"]",
)
PERSONAL_PATH_PATTERNS = (
    r"/home/[A-Za-z0-9_.-]+",
    r"/Users/[A-Za-z0-9_.-]+",
    r"(?i)[A-Z]:\\Users\\[A-Za-z0-9_.-]+",
)


def source_files():
    for path in ROOT.rglob("*"):
        if not path.is_file() or path.suffix not in SOURCE_SUFFIXES:
            continue
        if any(part in {".git", ".cache", "build", "node_modules", "__pycache__"} for part in path.parts):
            continue
        yield path


def scan(patterns, label):
    compiled = [re.compile(pattern) for pattern in patterns]
    findings = []
    for path in source_files():
        text = path.read_text(encoding="utf-8", errors="replace")
        for line_number, line in enumerate(text.splitlines(), 1):
            if any(pattern.search(line) for pattern in compiled):
                findings.append(f"{path.relative_to(ROOT)}:{line_number}")
    if findings:
        print(f"{label} found:")
        print("\n".join(findings))
        return 1
    print(f"{label} check passed.")
    return 0


def check_workflows():
    failures = []
    for path in sorted((ROOT / ".github/workflows").glob("*.yml")):
        lines = path.read_text(encoding="utf-8").splitlines()
        if any(line.endswith((" ", "\t")) for line in lines):
            failures.append(f"{path}: trailing whitespace")
        if any("\t" in line for line in lines):
            failures.append(f"{path}: tabs are not allowed")
        text = "\n".join(lines)
        for required in ("name:", "on:", "permissions:", "jobs:"):
            if required not in text:
                failures.append(f"{path}: missing {required}")
        if "uses: actions/checkout@" in text and "persist-credentials: false" not in text:
            failures.append(f"{path}: checkout must disable persisted credentials")
        if "pull_request_target" in text:
            failures.append(f"{path}: pull_request_target is not allowed")
    if failures:
        print("Workflow policy check failed:")
        print("\n".join(failures))
        return 1
    print("Workflow policy check passed.")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2 or sys.argv[1] not in {"secrets", "paths", "workflows"}:
        raise SystemExit("usage: check_repository_hygiene.py secrets|paths|workflows")
    if sys.argv[1] == "secrets":
        raise SystemExit(scan(SECRET_PATTERNS, "Credential scan"))
    if sys.argv[1] == "paths":
        raise SystemExit(scan(PERSONAL_PATH_PATTERNS, "Personal path scan"))
    raise SystemExit(check_workflows())
