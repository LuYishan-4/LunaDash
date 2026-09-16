#!/usr/bin/env python3
"""Check shipped Traditional Chinese lookups, catalog integrity and placeholders.

This checks static source strings, not arbitrary application names or log output.
Dynamic labels should use a literal at a translation call or a UI text property.
"""
import ast
from collections import Counter
import json
from pathlib import Path
import re
import sys

STRING = r'''(?:"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*')'''
TOKEN = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|' + STRING + r'|[A-Za-z_][A-Za-z_0-9]*|===|!==|==|!=|\|\||[^\s]')
PLACEHOLDER = re.compile(r'%L?[1-9][0-9]*|%n|%p%')


def literals(source, model_fields=()):
    tokens = [m.group() for m in TOKEN.finditer(source)
              if not m.group().startswith(('//', '/*'))]
    found = set()
    owners = []
    fields = {'title', 'description', 'label', 'message', 'placeholderText', *model_fields}
    for i, token in enumerate(tokens[:-1]):
        if token == '{':
            owners.append(tokens[i - 1] if i else '')
        elif token == '}' and owners:
            owners.pop()
        if token == 'translate' and tokens[max(0, i - 3):i] == ['QCoreApplication', ':', ':']:
            continue  # Qt's first argument is a context, not a source string.
        if token in ('tr', 'translate', 'finishWithError') and tokens[i + 1] == '(':
            depth, j = 1, i + 2
            while j < len(tokens) and depth:
                item = tokens[j]
                if item == '(':
                    depth += 1
                elif item == ')':
                    depth -= 1
                if item.startswith(('"', "'")) and (
                    j == i + 2 or tokens[j - 1] in ('?', ':', '||', '[', ',')
                ):
                    value = ast.literal_eval(item)
                    # C++ permits adjacent string literals in one argument.
                    while j + 1 < len(tokens) and tokens[j + 1].startswith(('"', "'")):
                        j += 1
                        value += ast.literal_eval(tokens[j])
                    found.add(value)
                j += 1
        if token in fields:
            if token == 'name' and owners and owners[-1] == 'LineIcon':
                continue  # Icon identifiers are assets, not translated labels.
            if i + 2 < len(tokens) and tokens[i + 1] == ':' and tokens[i + 2].startswith(('"', "'")):
                found.add(ast.literal_eval(tokens[i + 2]))
    return {text for text in found if re.search(r'[A-Za-z]', text)}


def load_catalog(path):
    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                if result[key] != value:
                    raise ValueError(f'{path}: conflicting duplicate key {key!r}')
                print(f'Warning: {path}: repeated identical key {key!r}', file=sys.stderr)
            result[key] = value
        return result
    data = json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=pairs)
    if not isinstance(data, dict):
        raise ValueError(f'{path}: expected a JSON object')
    for key, value in data.items():
        if not isinstance(key, str) or not isinstance(value, str) or not value.strip():
            raise ValueError(f'{path}: empty or non-string translation for {key!r}')
        if Counter(PLACEHOLDER.findall(key)) != Counter(PLACEHOLDER.findall(value)):
            raise ValueError(f'{path}: placeholders changed for {key!r}')
    return data


def main():
    root = Path(__file__).resolve().parents[1]
    directory = root / 'data/translations'
    catalogs = [directory / 'zh_TW.json', *sorted((directory / 'zh_TW').glob('*.json'))]
    messages = {}
    for path in catalogs:
        for source, value in load_catalog(path).items():
            if source in messages:
                raise ValueError(f'{path}: key already exists in another catalog: {source!r}')
            messages[source] = value
    missing = {}
    count = 0
    for folder in ('qml', 'src'):
        for path in sorted((root / folder).rglob('*')):
            if path.suffix not in ('.qml', '.cpp'):
                continue
            model_fields = {
                'SettingsCatalog.qml': ('name', 'pageName', 'keywords'),
                'SettingsPanel.qml': ('name',),
                'shortcuts.qml': ('name',),
                'Launcher.qml': ('name', 'genericName'),
            }.get(path.name, ())
            for text in literals(path.read_text(encoding='utf-8'), model_fields):
                count += 1
                if text not in messages:
                    missing.setdefault(text, []).append(str(path.relative_to(root)))
    for text, paths in sorted(missing.items()):
        print(json.dumps({'missing': text, 'files': paths}, ensure_ascii=False))
    print(f'Translation coverage: {count} static lookups, {len(messages)} entries, {len(missing)} missing.')
    return bool(missing)


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (ValueError, OSError, SyntaxError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
