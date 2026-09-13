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
    def test_warning_and_security_findings_are_rejected(self):
        report = {'runs': [{'tool': {'driver': {'rules': [
            {'id': 'security', 'properties': {'security-severity': '8.0'}}]}},
            'results': [{'ruleId': 'crash', 'level': 'warning'},
                        {'ruleId': 'security', 'level': 'note'}]}]}
        self.assertEqual(gate.findings_in(report), ['crash', 'security'])

    def test_missing_output_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            result = subprocess.run([sys.executable, str(SCRIPT), directory], capture_output=True)
            self.assertNotEqual(result.returncode, 0)

    def test_cli_clean_and_vulnerable_results(self):
        with tempfile.TemporaryDirectory() as directory:
            report = pathlib.Path(directory) / 'cpp.sarif'
            for findings, expected in [([], 0), ([{'ruleId': 'use-after-free', 'level': 'error'}], 1)]:
                report.write_text(json.dumps({'version': '2.1.0', 'runs': [{'tool': {'driver': {'name': 'CodeQL'}}, 'results': findings}]}))
                result = subprocess.run([sys.executable, str(SCRIPT), directory], capture_output=True)
                self.assertEqual(result.returncode, expected)


if __name__ == '__main__':
    unittest.main()
