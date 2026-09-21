import importlib.util
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
SCRIPT = ROOT / 'scripts/security/check_sarif.py'
spec = importlib.util.spec_from_file_location('sarif_gate', SCRIPT)
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)


class SarifGateTests(unittest.TestCase):
    def test_errors_and_security_findings_are_rejected_but_quality_warnings_are_not(self):
        report = {'runs': [{'tool': {'driver': {'rules': [
            {'id': 'security', 'properties': {'security-severity': '8.0'}},
            {'id': 'quality'}]}},
            'results': [{'ruleId': 'quality', 'level': 'warning'},
                        {'ruleId': 'crash', 'level': 'error'},
                        {'ruleId': 'security', 'level': 'note'}]}]}
        self.assertEqual(gate.findings_in(report), ['crash', 'security'])

    def test_missing_output_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            result = subprocess.run([sys.executable, str(SCRIPT), directory], capture_output=True)
            self.assertNotEqual(result.returncode, 0)

    def test_cli_clean_quality_warning_and_vulnerable_results(self):
        with tempfile.TemporaryDirectory() as directory:
            report = pathlib.Path(directory) / 'cpp.sarif'
            cases = [
                ([], 0),
                ([{'ruleId': 'quality', 'level': 'warning'}], 0),
                ([{'ruleId': 'use-after-free', 'level': 'error'}], 1),
            ]
            for findings, expected in cases:
                report.write_text(json.dumps({
                    'version': '2.1.0',
                    'runs': [{'tool': {'driver': {'name': 'CodeQL'}}, 'results': findings}],
                }))
                result = subprocess.run([sys.executable, str(SCRIPT), directory], capture_output=True)
                self.assertEqual(result.returncode, expected)


if __name__ == '__main__':
    unittest.main()
