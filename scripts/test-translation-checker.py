#!/usr/bin/env python3
"""Offline regression tests for extraction and catalog validation."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('checker', Path(__file__).with_name('check-translations.py'))
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)


class TranslationChecks(unittest.TestCase):
    def test_direct_and_concatenated_literals(self):
        self.assertEqual(checker.literals('shell.tr("Hello"); translate("Long " "message");'), {'Hello', 'Long message'})

    def test_ternaries_and_arrays(self):
        self.assertEqual(checker.literals('shell.tr(kind === "input" ? "Input volume" : "Output volume")'), {'Input volume', 'Output volume'})
        self.assertEqual(checker.literals('shell.tr(["First", "Second"][i])'), {'First', 'Second'})

    def test_comments_and_qt_context_are_not_messages(self):
        self.assertEqual(checker.literals('// tr("Ignored")\n/* translate("Ignored") */'), set())
        self.assertEqual(checker.literals('QCoreApplication::translate("Context", source)'), set())

    def test_model_and_help_labels(self):
        self.assertEqual(checker.literals('HelpText { message: "Help" }'), {'Help'})
        self.assertEqual(checker.literals('{name:"Action", keywords:"key binding"}', ('name', 'keywords')), {'Action', 'key binding'})

    def test_icon_names_are_not_ui_labels(self):
        source = 'LineIcon { name: "search" } property var entries: [{name:"Search"}]'
        self.assertEqual(checker.literals(source, ('name',)), {'Search'})

    def validate(self, content):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'catalog.json'
            path.write_text(content, encoding='utf-8')
            return checker.load_catalog(path)

    def test_valid_placeholders(self):
        self.assertEqual(self.validate(json.dumps({'Item %1 at %2': '%2: item %1'})), {'Item %1 at %2': '%2: item %1'})

    def test_reject_changed_placeholders(self):
        for target in ('Item %2', 'Item', 'Item %1 %1'):
            with self.assertRaises(ValueError):
                self.validate(json.dumps({'Item %1': target}))

    def test_reject_conflicting_duplicates_and_empty_values(self):
        for value in ('{"A":"one","A":"two"}', '{"A":""}', '{"A":2}'):
            with self.assertRaises(ValueError):
                self.validate(value)


if __name__ == '__main__':
    unittest.main()
