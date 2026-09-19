"""UI translations belong in data packs, not C++ or QML source."""
import pathlib
import re
import sys
root = pathlib.Path(__file__).resolve().parents[2]
violations = []
translation_link = re.compile(r'\[[^\]\n]+\]\((docs/readme/README\.[a-z]{2}(?:-[A-Za-z]{2,4})?\.md)\)')
for directory in ('src', 'include', 'qml'):
    for path in (root / directory).rglob('*'):
        if path.suffix in {'.cpp', '.c', '.h', '.hpp', '.qml'} and re.search(r'[\u3400-\u9fff]', path.read_text()):
            violations.append(str(path.relative_to(root)))
for path in [root / 'README.md', root / 'AGENTS.md', *(root / 'docs').glob('*.md'),
             root / '.github/pull_request_template.md', *(root / 'site').glob('*'), *(root / 'site/src').rglob('*')]:
    # Explicitly requested translated guide; source and primary docs stay English.
    if path == root / 'docs/LOGIN_SESSION.zh-TW.md':
        continue
    if path.is_file() and path.suffix in {'.md', '.html', '.css', '.js', '.ts', '.astro'}:
        content = path.read_text()
        if path == root / 'README.md':
            # Language names may use their own script when linking to an existing translation.
            content = translation_link.sub(
                lambda match: f'[]({match[1]})' if (root / match[1]).is_file() else match[0],
                content,
            )
        if re.search(r'[\u3400-\u9fff]', content):
            violations.append(str(path.relative_to(root)))
if violations:
    print('Source, documentation or website contains non-English text:', ', '.join(violations))
    sys.exit(1)
print('Source language policy passed.')
