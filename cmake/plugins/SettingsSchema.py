"""SDK counterpart of core/settings/SettingsSchema.hpp; test with shared fixtures."""
import math
import re

LIMIT = 9007199254740991


def finite(value):
    return type(value) in (int, float) and abs(value) <= LIMIT and math.isfinite(value)


def same(a, b):
    return (type(a) is type(b) or (type(a) in (int, float) and type(b) in (int, float))) and a == b


def member(value, values):
    return any(same(value, entry) for entry in values)


def control(rule):
    if "control" in rule:
        return rule["control"]
    if "enum" in rule:
        return "select"
    if rule.get("type") == "boolean":
        return "toggle"
    if rule.get("type") in ("integer", "number"):
        return "slider" if "minimum" in rule and "maximum" in rule else "number"
    return "text"


def valid_value(rule, value):
    kind = rule.get("type")
    if kind == "boolean":
        valid = type(value) is bool
    elif kind == "string":
        valid = isinstance(value, str) and len(value.encode('utf-16-le')) // 2 <= 4096
        if valid and "pattern" in rule:
            valid = re.search(rule["pattern"], value) is not None
    elif kind in ("integer", "number"):
        valid = finite(value) and (kind != "integer" or value == math.floor(value))
        valid = valid and (member(value, rule.get("specialValues", [])) or
                          (value >= rule.get("minimum", -LIMIT) and value <= rule.get("maximum", LIMIT)))
    elif kind == "array":
        valid = isinstance(value, list) and len(value) <= rule.get("maxItems", 64)
        valid = valid and all(valid_value(rule["items"], x) and not member(x, value[:i])
                              for i, x in enumerate(value))
    else:
        valid = False
    return valid and ("enum" not in rule or member(value, rule["enum"]))


def validate_schema(schema):
    def require(ok, message):
        if not ok:
            raise ValueError("Invalid settings schema: " + message)
    require(isinstance(schema, dict) and len(schema) <= 128, "fields")
    for key, rule in schema.items():
        require(re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", key) and isinstance(rule, dict), key)
        kind = rule.get("type")
        numeric = kind in ("integer", "number")
        require(kind in ("boolean", "string", "integer", "number", "array") and "default" in rule, key)
        for name in ("label", "description", "group", "pattern", "control"):
            require(name not in rule or isinstance(rule[name], str) and len(rule[name].encode('utf-16-le')) // 2 <= 4096, key)
        require("readOnly" not in rule or type(rule["readOnly"]) is bool, key)
        for name in ("minimum", "maximum", "step", "order"):
            if name in rule:
                n = rule[name]
                require(finite(n) and (name == "order" or numeric), key)
                require(name != "step" or n > 0 and (kind != "integer" or n == math.floor(n)), key)
        require(not ("minimum" in rule and "maximum" in rule) or rule["minimum"] <= rule["maximum"], key)
        if "pattern" in rule:
            require(kind == "string", key)
            re.compile(rule["pattern"])
        if "specialValues" in rule:
            values = rule["specialValues"]
            require(numeric and isinstance(values, list) and len(values) <= 16, key)
            require(all(finite(n) and (kind != "integer" or n == math.floor(n)) for n in values), key)
        if kind == "array":
            items = rule.get("items", {})
            require(isinstance(items, dict) and items.get("type") == "string" and
                    isinstance(items.get("enum"), list) and len(items["enum"]) > 0, key)
            n = rule.get("maxItems", 64)
            require(finite(n) and n == math.floor(n) and 0 <= n <= 64, key)
            validate_schema({"item": dict(items, default=items["enum"][0])})
        if "enum" in rule:
            values = rule["enum"]
            require(isinstance(values, list) and 0 < len(values) <= 128, key)
            base = {k: v for k, v in rule.items() if k != "enum"}
            require(all(valid_value(base, x) and not member(x, values[:i]) for i, x in enumerate(values)), key)
        widget = control(rule)
        require((widget == "toggle" and kind == "boolean") or
                (widget == "select" and ("enum" in rule or kind == "array")) or
                (widget == "number" and numeric) or
                (widget == "slider" and numeric and "minimum" in rule and "maximum" in rule and "specialValues" not in rule) or
                (widget == "text" and kind == "string"), key)
        require(valid_value(rule, rule["default"]), key)
