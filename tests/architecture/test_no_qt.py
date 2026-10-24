"""LunaDash's compositor, desktop services and native tests must not use Qt.

The independent LunaDash-Shell repository (Quickshell/QML) is a separate
Wayland client and is not part of this check. Environment variables that the
compositor forwards to third-party Qt applications are configuration strings,
not Qt linkage, and remain allowed.
"""

import re
from pathlib import Path

root = Path(__file__).resolve().parents[2]
source_roots = [root / "src", root / "tests"]
native_suffixes = {".c", ".cc", ".cpp", ".h", ".hpp"}
patterns = (
    re.compile(r"#\s*include\s*[<\"]Q[A-Za-z]"),
    re.compile(r"#\s*include\s*[<\"]Qt[A-Za-z]*/"),
    re.compile(r"\bQ_OBJECT\b|\bQ_SLOTS\b|\bQ_SIGNALS\b|\bQTEST_\w*MAIN\b"),
    re.compile(r"\bQ(?:String|ByteArray|Json\w*|Rect|Size|Point|Url|Dir|File\w*|"
               r"Object|Timer|Process|CoreApplication|Settings|Variant\w*|"
               r"StringList|List|Map|Vector|Hash|Set|TemporaryDir|SaveFile|"
               r"DBus\w*|Test)\b"),
    re.compile(r"\bq(?:u?int(?:8|16|32|64)|real|sizetype|ulonglong|longlong)\b"),
    re.compile(r"\bq(?:Debug|Warning|Info|Critical|Fatal|putenv|getenv|"
               r"EnvironmentVariable\w*|Abs|Max|Min|Bound|Round)\s*\("),
)

violations = []
for base in source_roots:
    for path in sorted(base.rglob("*")):
        if not path.is_file() or path.suffix not in native_suffixes:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for number, line in enumerate(text.splitlines(), 1):
            stripped = line.strip()
            if stripped.startswith("//") or stripped.startswith("*"):
                continue
            for pattern in patterns:
                if pattern.search(line):
                    violations.append(
                        f"{path.relative_to(root)}:{number}: {stripped}")
                    break

# The plugin SDK may bake shaders for OpenGL plugins that the independent
# Quickshell shell loads; that optional tool runs only for those third-party
# plugin builds and is never part of the LunaDash build itself.
cmake_files = [root / "CMakeLists.txt", *sorted((root / "cmake").rglob("*.cmake"))]
cmake_files = [path for path in cmake_files
               if path.relative_to(root).parts[:2] != ("cmake", "plugins")]
for path in cmake_files:
    text = path.read_text(encoding="utf-8")
    for number, line in enumerate(text.splitlines(), 1):
        if line.strip().startswith("#"):
            continue
        if re.search(r"find_package\(\s*Qt|Qt6::|Qt5::|\bAUTOMOC\b|"
                     r"\bAUTOUIC\b|\bAUTORCC\b|qt_add_|qt6_", line):
            violations.append(f"{path.relative_to(root)}:{number}: {line.strip()}")

assert not violations, "Qt usage remains:\n" + "\n".join(violations)
print("No Qt headers, types, macros or CMake linkage remain in LunaDash sources.")
