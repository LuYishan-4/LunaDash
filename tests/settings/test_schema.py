#!/usr/bin/env python3
"""SDK/runtime fixtures and the shipped module/plugin settings contracts."""
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
        for directory in ('data/plugins', 'templates/plugins', 'qml/plugins'):
            for path in (ROOT / directory).rglob('metadata.json'):
                manifest = json.loads(path.read_text())
                if manifest.get('schemaVersion') == 2:
                    validate_schema(manifest['settings'])

    def test_module_registry(self):
        registry = json.loads((ROOT / 'data/modules/registry.json').read_text())
        self.assertEqual(registry['schemaVersion'], 1)
        ids = set()
        for module in registry['modules']:
            for section, schema in registry['common'].items():
                module['sections'][section] = dict(schema, **module['sections'][section])
            self.assertNotIn(module['id'], ids)
            ids.add(module['id'])
            self.assertTrue(module['type'])
            self.assertEqual(set(module['sections']), {'module','style','config','custom'})
            for schema in module['sections'].values():
                validate_schema(schema)
            if module['recovery']:
                rule = module['sections']['module']['enabled']
                self.assertTrue(rule['readOnly'])
                self.assertFalse(valid_value(rule, False))
            custom = module['sections']['custom']['entry']
            self.assertTrue(valid_value(custom, module['id'] + '/Main.qml'))
            self.assertFalse(valid_value(custom, '../Main.qml'))
            self.assertFalse(valid_value(custom, '/tmp/Main.qml'))
        self.assertEqual(len(ids), 11)
        self.assertTrue({'orbit', 'dock'}.issubset(ids))

    def test_plugin_controls_are_covered_without_bundled_plugins(self):
        controls = set()
        for path in (ROOT / 'templates/plugins').rglob('metadata.json'):
            manifest = json.loads(path.read_text())
            if manifest.get('schemaVersion') != 2:
                continue
            for rule in manifest.get('settings', {}).values():
                controls.add(control(rule))
        # Template coverage plus the shared contract must exercise every stable
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
