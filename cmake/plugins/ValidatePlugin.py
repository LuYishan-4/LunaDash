#!/usr/bin/env python3
"""Validate a LunaDash SDK 2 manifest and generate its build receipt / C ABI entry."""
import argparse
import hashlib
import json
from pathlib import Path
import re
from SettingsSchema import control, validate_schema


def validate(manifest, root, targets):
    def require(test, message):
        if not test:
            raise ValueError(message)

    def local(name, suffix, existing=True):
        require(isinstance(name, str) and re.fullmatch(r"[A-Za-z0-9_.-]+", name)
                and name not in (".", "..") and name.endswith(suffix),
                f"Expected a local {suffix} filename: {name}")
        path = root / name
        if existing:
            require(path.is_file() and path.resolve().parent == root.resolve()
                    and path.stat().st_size <= 1024 * 1024,
                    f"Missing, oversized or escaping plugin file: {name}")
        return name

    require(manifest.get("schemaVersion") == 2, "schemaVersion must be 2")
    require(manifest.get("sdk") == {"name": "LunaDash", "apiVersion": 2},
            "Declare sdk: {name: LunaDash, apiVersion: 2}")
    require(isinstance(manifest.get("id"), str) and
            re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]+", manifest["id"]), "Invalid plugin ID")
    for key in ("name", "version"):
        require(isinstance(manifest.get(key), str) and manifest[key], f"Missing {key}")
    icon = manifest.get("icon", "applications-system")
    require(isinstance(icon, str) and re.fullmatch(r"[A-Za-z0-9_.-]+", icon),
            "icon must be a theme name or local image filename")
    if re.search(r"\.(png|jpe?g|webp|svg)$", icon, re.IGNORECASE):
        path = root / icon
        require(path.is_file() and path.resolve().parent == root.resolve()
                and path.stat().st_size <= 1024 * 1024,
                f"Missing, oversized or escaping plugin icon: {icon}")
    tags = manifest.get("tags", [])
    require(isinstance(tags, list) and len(tags) <= 12 and
            all(isinstance(tag, str) and tag.strip() == tag and
                0 < len(tag) <= 32 and not any(ord(ch) < 32 for ch in tag)
                for tag in tags) and len(set(tags)) == len(tags),
            "tags must be up to 12 unique non-empty strings of at most 32 characters")
    kind = manifest.get("type")
    require(kind in ("effect", "quickshell", "opengl"), "Unknown plugin type")
    target = next((t for t in targets if t["id"] == manifest.get("target")), None)
    require(target and kind in target["types"], "Target does not support this plugin type")
    require(manifest.get("mode") in ("replace", "augment"), "mode must be replace or augment")
    require(isinstance(manifest.get("enabledByDefault", False), bool), "enabledByDefault must be boolean")
    require(kind != "effect" or not manifest.get("enabledByDefault"), "Native effects must default to disabled")
    require("layoutMode" not in manifest,
            "layoutMode was removed; use windowTemplate")
    window_template = manifest.get("windowTemplate", "tiling")
    require(window_template in ("tiling", "stacking"), "Unknown windowTemplate")
    require("windowTemplate" not in manifest or
            (kind == "effect" and target["id"] == "window-layout"),
            "windowTemplate requires a native window-layout plugin")
    require(window_template != "stacking" or manifest["mode"] == "replace",
            "Stacking windowTemplate requires replacement mode")
    schema = manifest.get("settings")
    require(isinstance(schema, dict), "settings must be an object (empty is allowed)")
    validate_schema(schema)
    # Plugin settings are rendered automatically by the shared QML settings
    # surface. Keep the plugin SDK to the four stable controls so native
    # effects and visual plugins never require custom settings QML.
    allowed_controls = {"toggle", "select", "number", "slider"}
    require(all(control(rule) in allowed_controls for rule in schema.values()),
            "Plugin settings support only toggle, select, number and slider controls")
    if kind == "opengl":
        shaders = manifest.get("shaders", {})
        require(isinstance(shaders, dict), "shaders must be an object")
        local(shaders.get("vertex"), ".vert")
        local(shaders.get("fragment"), ".frag")
    else:
        local(manifest.get("entry"), ".qml" if kind == "quickshell" else ".so", kind == "quickshell")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--metadata", type=Path, required=True)
    parser.add_argument("--targets", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        data = args.metadata.read_bytes()
        if len(data) > 65536:
            raise ValueError("metadata.json exceeds 64 KiB")
        manifest = json.loads(data)
        validate(manifest, args.metadata.parent, json.loads(args.targets.read_text()))
        args.output.mkdir(parents=True, exist_ok=True)
        (args.output / "metadata.json").write_bytes(data)
        receipt = {"apiVersion": 2, "metadataSha256": hashlib.sha256(data).hexdigest()}
        (args.output / ".lunadash-sdk.json").write_text(json.dumps(receipt) + "\n")
        if manifest["type"] == "effect":
            # Escape a UTF-8 JSON byte stream as portable C string bytes.
            literal = '"' + ''.join(f'\\{byte:03o}' for byte in data) + '"'
            (args.output / "Registration.c").write_text(
                '#include "core/plugins/PluginApi.h"\n'
                'LUDASH_PLUGIN_EXPORT const ludash_plugin_api *ludash_plugin_entry_v2(void) {\n'
                f'  static const ludash_plugin_api api = {{sizeof(ludash_plugin_api), 2u, {literal}, ludash_plugin_process}};\n'
                '  return &api;\n}\n')
    except (OSError, ValueError, TypeError, KeyError, re.error) as error:
        parser.exit(1, f"LunaDash plugin metadata: {error}\n")


if __name__ == "__main__":
    main()
