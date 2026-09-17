#!/usr/bin/env python3
"""Validate every shipped LunaDash JSON language pack.

Static source strings are extracted from C++ and QML. Traditional Chinese is the
reference complete translation and must cover every extracted UI string. Other
locale packs may be introduced incrementally, but their JSON integrity,
cross-catalog uniqueness and placeholders are always enforced and their coverage
is reported. Adding another <locale>.json automatically enrolls it in CI.
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
LOCALE_FILE = re.compile(r'^[a-z]{2,3}_[A-Z]{2}\.json$')
REFERENCE_COMPLETE_LOCALES = {'zh_TW'}


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
            continue
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
                    while j + 1 < len(tokens) and tokens[j + 1].startswith(('"', "'")):
                        j += 1
                        value += ast.literal_eval(tokens[j])
                    found.add(value)
                j += 1
        if token in fields:
            if token == 'name' and owners and owners[-1] == 'LineIcon':
                continue
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


def merge_catalogs(paths):
    """Report every cross-file duplicate without hiding later diagnostics."""
    messages, origins, errors = {}, {}, []
    for path in paths:
        for source, value in load_catalog(path).items():
            if source in messages:
                errors.append(f'{path}: key already exists in {origins[source]}: {source!r}')
                continue
            messages[source] = value
            origins[source] = path
    return messages, errors


def catalogs_for(directory, locale):
    paths = []
    root = directory / f'{locale}.json'
    if root.exists():
        paths.append(root)
    feature_dir = directory / locale
    if feature_dir.is_dir():
        paths.extend(sorted(feature_dir.glob('*.json')))
    return paths


def discover_locales(directory):
    return sorted(path.stem for path in directory.glob('*.json')
                  if LOCALE_FILE.match(path.name) and path.stem != 'en_US')


def source_messages(root):
    found = {}
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
                found.setdefault(text, []).append(str(path.relative_to(root)))
    return found


def main():
    root = Path(__file__).resolve().parents[1]
    directory = root / 'data/translations'
    sources = source_messages(root)
    locales = discover_locales(directory)
    fatal = False

    if not locales:
        print('No non-English translation packs were found.', file=sys.stderr)
        return True
    for required in sorted(REFERENCE_COMPLETE_LOCALES):
        if required not in locales:
            print(f'Required complete locale is missing: {required}', file=sys.stderr)
            fatal = True

    for locale in locales:
        catalogs = catalogs_for(directory, locale)
        messages, errors = merge_catalogs(catalogs)
        for error in errors:
            print(f'[{locale}] {error}', file=sys.stderr)
        if errors:
            fatal = True

        missing = {text: paths for text, paths in sources.items() if text not in messages}
        translated = len(sources) - len(missing)
        coverage = 100.0 if not sources else translated * 100.0 / len(sources)
        print(f'{locale}: {translated}/{len(sources)} static messages translated ({coverage:.1f}%), '
              f'{len(messages)} catalog entries, {len(errors)} duplicate errors.')

        if locale in REFERENCE_COMPLETE_LOCALES and missing:
            fatal = True
            for text, paths in sorted(missing.items()):
                print(json.dumps({'locale': locale, 'missing': text, 'files': paths}, ensure_ascii=False))

    return fatal


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (ValueError, OSError, SyntaxError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
