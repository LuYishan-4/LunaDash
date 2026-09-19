"""Require valid Qt guards to pass while real use-after-free still fails."""
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile


root = Path(__file__).resolve().parents[2]
analyzer = sys.argv[1] if len(sys.argv) > 1 else "clang-tidy"
cflags = shlex.split(subprocess.check_output(
    ["pkg-config", "--cflags", "Qt6Core"], text=True))
# CMake's imported Qt targets use system includes. Clang 18 misdiagnoses
# QPointer destruction with these flags, even without any LuDash code.
flags = ["-std=c++20", "-fPIC", "-DQT_NO_DEBUG"]
for flag in cflags:
    flags.extend(["-isystem", flag[2:]] if flag.startswith("-I") else [flag])

sources = {
    "valid_guard": """#include <QObject>
#include <QPointer>
namespace LunaDash {
void watch(QObject* object) { const QPointer<QObject> guard(object); }
}
""",
    "invalid_lifetime": """namespace LunaDash {
int readAfterFree() { int* value = new int(7); delete value; return *value; }
}
""",
}
with tempfile.TemporaryDirectory(prefix="ludash-analyzer-") as directory:
    for name, source in sources.items():
        path = Path(directory) / (name + ".cpp")
        path.write_text(source, encoding="utf-8")
        result = subprocess.run(
            [analyzer, str(path), "--warnings-as-errors=*", "--config-file=" + str(root / ".clang-tidy"),
             "--extra-arg-before=--driver-mode=g++", "--", *flags],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            timeout=60, check=False)
        if name == "valid_guard":
            assert result.returncode == 0, result.stdout
        else:
            assert result.returncode != 0, "The analyzer missed use-after-free."
            assert "[clang-analyzer-cplusplus.NewDelete" in result.stdout, result.stdout
print("Analyzer checks passed: valid Qt guard accepted; use-after-free rejected.")
