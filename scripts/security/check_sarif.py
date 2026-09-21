#!/usr/bin/env python3
"""Fail closed on CodeQL security/error findings; never print source snippets."""
import json
import pathlib
import sys


def findings_in(document):
    findings = []
    for run in document.get("runs", []):
        rules = {rule["id"]: rule for rule in run.get("tool", {}).get("driver", {}).get("rules", [])}
        for result in run.get("results", []):
            rule_id = result.get("ruleId", "unknown-rule")
            rule = rules.get(rule_id, {})
            level = result.get("level", rule.get("defaultConfiguration", {}).get("level", "warning"))
            security = rule.get("properties", {}).get("security-severity")
            # Keep security-and-quality enabled so quality findings remain
            # visible as CodeQL annotations, but only security-tagged results
            # or explicit SARIF errors should block a pull request.
            if level == "error" or security is not None:
                findings.append(rule_id)
    return findings


def main():
    if len(sys.argv) != 2:
        raise SystemExit("Usage: check_sarif.py <SARIF directory>")
    paths = list(pathlib.Path(sys.argv[1]).rglob("*.sarif"))
    if not paths:
        raise SystemExit("No SARIF output found; refusing to mark analysis as passed.")
    findings = []
    for path in paths:
        with path.open(encoding="utf-8") as source:
            document = json.load(source)
            if not document.get("runs") or document.get("version") != "2.1.0":
                raise SystemExit("Invalid or empty SARIF report; refusing to pass.")
            findings.extend(findings_in(document))
    if findings:
        print(f"Security gate failed: {len(findings)} security/error findings. Review the CodeQL annotations.")
        raise SystemExit(1)
    print("CodeQL SARIF gate passed: no security-severity findings or errors.")


if __name__ == "__main__":
    main()
