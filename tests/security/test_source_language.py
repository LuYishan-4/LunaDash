"""UI translations belong in data packs, not C++ or QML source."""
import pathlib
import re
import sys
root = pathlib.Path(__file__).resolve().parents[2]
violations = []
for directory in ('src', 'include', 'qml'):
    for path in (root / directory).rglob('*'):
        if path.suffix in {'.cpp', '.h', '.qml'} and re.search(r'[\u3400-\u9fff]', path.read_text()):
            violations.append(str(path.relative_to(root)))
if violations:
    print('Source contains non-English UI text:', ', '.join(violations))
    sys.exit(1)
print('Source language policy passed.')
