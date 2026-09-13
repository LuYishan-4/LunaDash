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
for path in [root / 'README.md', root / 'AGENTS.md', *(root / 'docs').glob('*.md'),
             root / '.github/pull_request_template.md', *(root / 'site').glob('*'), *(root / 'site/src').glob('*.ts')]:
    if path.is_file() and path.suffix in {'.md', '.html', '.css', '.js', '.ts'} and re.search(r'[\u3400-\u9fff]', path.read_text()):
        violations.append(str(path.relative_to(root)))
if violations:
    print('Source, documentation or website contains non-English text:', ', '.join(violations))
    sys.exit(1)
print('Source language policy passed.')
