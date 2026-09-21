#!/usr/bin/env python3
"""Create a LunaDash SDK 2 plugin from a registered desktop feature template."""
import argparse
import json
from pathlib import Path
import re
import shutil
import sys


def main():
    source = Path(__file__).resolve().parents[1]
    installed = source / "share/lunadash/plugin-sdk"
    templates = source / "templates/plugins"
    registry = source / "data/plugins/targets.json"
    if not templates.is_dir():
        templates = installed / "templates"
        registry = installed / "targets.json"
    targets = json.loads(registry.read_text())
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--type", choices=["effect", "quickshell", "opengl"])
    parser.add_argument("--target")
    parser.add_argument("--id")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.list:
        for target in targets:
            print(f'{target["category"]:16} {target["id"]:24} {", ".join(target["types"])}')
        return 0
    target = next((entry for entry in targets if entry["id"] == args.target), None)
    if not target or args.type not in target["types"]:
        parser.error("Choose a supported type/target pair from --list")
    if not args.id or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]+", args.id):
        parser.error("--id must be a reverse-domain plugin identifier")
    if not args.output or args.output.exists():
        parser.error("--output must name a new directory")
    if args.type == "effect" and args.target == "window-layout":
        template = "window-template"
    elif args.type == "effect" and args.target == "window-animation":
        template = "window-animation"
    else:
        template = "effect-cpp" if args.type == "effect" else args.type
    shutil.copytree(templates / template, args.output)
    path = args.output / "metadata.json"
    manifest = json.loads(path.read_text())
    manifest.update(id=args.id, target=args.target, name=args.target + " plugin")
    if args.type == "effect" and template == "effect-cpp":
        manifest["settings"] = {}
        (args.output / "Effect.cpp").write_text('''#include "core/plugins/PluginApi.h"
#include <QJsonDocument>
#include <QJsonObject>
namespace LunaDash {
extern "C" int ludash_plugin_process(const char *request, char *response, size_t capacity) {
    const auto input = QJsonDocument::fromJson(request).object();
    auto output = input.value(input.value("mode").toString() == "replace" ? "builtin" : "current").toObject();
    // Customize the feature result here. See PLUGIN_TARGETS.md for its contract.
    const auto bytes = QJsonDocument(output).toJson(QJsonDocument::Compact);
    return ludash_plugin_write_json(bytes.constData(), response, capacity);
}
} // namespace LunaDash
''')
    path.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Created {args.id} ({args.type} / {args.target}) in {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
