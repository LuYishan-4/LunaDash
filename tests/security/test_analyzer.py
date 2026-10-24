"""Require valid ownership to pass while real use-after-free still fails."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile


root = Path(__file__).resolve().parents[2]
analyzer = sys.argv[1] if len(sys.argv) > 1 else "clang-tidy"
flags = ["-std=c++20", "-fPIC"]

sources = {
    "valid_ownership": """#include <memory>
namespace LunaDash {
int readOwned() { const auto value = std::make_unique<int>(7); return *value; }
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
        line_filter = json.dumps([{
            "name": str(path),
            "lines": [[1, max(1, len(source.splitlines()))]],
        }])

        warnings_as_errors = "*"
        result = subprocess.run(
            [analyzer, str(path),
             "--warnings-as-errors=" + warnings_as_errors,
             "--config-file=" + str(root / ".clang-tidy"),
             "--line-filter=" + line_filter,
             "--extra-arg-before=--driver-mode=g++", "--", *flags],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            timeout=60, check=False)

        if name == "valid_ownership":
            assert result.returncode == 0, result.stdout
        else:
            assert result.returncode != 0, "The analyzer missed use-after-free."
            assert "[clang-analyzer-cplusplus.NewDelete" in result.stdout, result.stdout

print("Analyzer checks passed: valid ownership accepted; use-after-free rejected.")
