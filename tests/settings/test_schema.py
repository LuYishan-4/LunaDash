#!/usr/bin/env python3
"""SDK/runtime fixtures and the shipped plugin settings contracts."""
import importlib.util
import json
from pathlib import Path
import re
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'cmake/plugins'))
from SettingsSchema import control, valid_value, validate_schema


class SettingsSchemaTests(unittest.TestCase):
    def test_shared_fixtures(self):
        for index, case in enumerate(json.loads((Path(__file__).with_name('contract.json')).read_text())):
            with self.subTest(index=index):
                schema = {'option': case['rule']}
                if not case.get('valid', True):
                    with self.assertRaises((ValueError, TypeError, re.error)):
                        validate_schema(schema)
                    continue
                validate_schema(schema)
                self.assertEqual(control(case['rule']), case['control'])
                for value in case['good']:
                    self.assertTrue(valid_value(case['rule'], value), repr(value))
                for value in case['bad']:
                    self.assertFalse(valid_value(case['rule'], value), repr(value))

    def test_all_shipped_schemas(self):
        for target in json.loads((ROOT / 'data/plugins/targets.json').read_text()):
            validate_schema(target['settings'])
        for path in (ROOT / 'data/plugins').rglob('metadata.json'):
            manifest = json.loads(path.read_text())
            if manifest.get('schemaVersion') == 2:
                validate_schema(manifest['settings'])

    def test_plugin_controls_are_covered_without_bundled_plugins(self):
        controls = set()
        create = importlib.util.spec_from_file_location(
            'create_plugin', ROOT / 'scripts/create-plugin.py')
        generator = importlib.util.module_from_spec(create)
        create.loader.exec_module(generator)
        for target in ('window-animation', 'window-rules', 'window-layout'):
            manifest = json.loads(generator.scaffold(target, 'org.example.test')['metadata.json'])
            validate_schema(manifest['settings'])
            for rule in manifest['settings'].values():
                controls.add(control(rule))
        # Generated projects plus the shared contract must exercise every stable
        # plugin control even though runtime example plugins are not bundled.
        for case in json.loads((Path(__file__).with_name('contract.json')).read_text()):
            if case.get('valid', True) and case.get('control') in {
                    'toggle', 'select', 'number', 'slider'}:
                controls.add(case['control'])
        self.assertEqual(controls, {'toggle','select','number','slider'})

    def test_nonfinite_and_oversized(self):
        rule = {'type':'number','default':0}
        for value in (float('nan'), float('inf'), -float('inf'), 2**53):
            self.assertFalse(valid_value(rule, value))
        with self.assertRaises(ValueError):
            validate_schema({'x':dict(rule, step=float('nan'))})
        with self.assertRaises(ValueError):
            validate_schema({f'x{i}':rule for i in range(129)})
        with self.assertRaises(ValueError):
            validate_schema({'../path':rule})


if __name__ == '__main__':
    unittest.main()
