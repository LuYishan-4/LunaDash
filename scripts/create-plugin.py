#!/usr/bin/env python3
"""Create a native LunaDash SDK 2 plugin project for a compositor feature.

The scaffolds are written by this script; there is no template directory. Every native plugin
is an `effect`: a shared library whose `ludash_plugin_process` receives the feature's JSON
request and returns its result (see PLUGIN_TARGETS.md).
"""
import argparse
import json
from pathlib import Path
import re
import sys

VERSION = "1.0.2a"

# Example settings per target. Together they use every stable control (toggle, select,
# number, slider), so generated projects double as the SDK's control coverage.
TARGET_SETTINGS = {
    "window-animation": {
        "duration": {"type": "integer", "default": 240, "minimum": 0, "maximum": 600,
                     "control": "number", "label": "Duration (ms)"},
        "enterOffset": {"type": "integer", "default": 12, "minimum": -100, "maximum": 100,
                        "control": "number", "label": "Enter offset"},
        "focusOpacity": {"type": "number", "default": 0.82, "minimum": 0, "maximum": 1,
                         "control": "slider", "label": "Unfocused opacity"},
        "exitScale": {"type": "number", "default": 0.9, "minimum": 0.5, "maximum": 1,
                      "control": "slider", "label": "Exit scale"},
        "easing": {"type": "string", "default": "outCubic",
                   "enum": ["linear", "outCubic", "outQuint", "inOutCubic"],
                   "control": "select", "label": "Easing"},
    },
    "window-rules": {
        "workspace": {"type": "integer", "default": 1, "minimum": 0, "maximum": 9, "step": 1,
                      "control": "number", "label": "Workspace"},
        "maximize": {"type": "boolean", "default": False, "control": "toggle",
                     "label": "Start maximized"},
    },
}

CMAKE = """cmake_minimum_required(VERSION 3.21)
project({target} LANGUAGES C CXX)
find_package(LunaDashPlugin 2 CONFIG REQUIRED)
{json_line}lunadash_add_plugin({target} METADATA "${{CMAKE_CURRENT_SOURCE_DIR}}/metadata.json" SOURCES {source})
{link_line}"""

CPP_SOURCE = """#include "core/plugins/PluginApi.h"
#include <nlohmann/json.hpp>

namespace LunaDash {{
// Receives the feature request as JSON and returns the feature result. In "augment" mode the
// result starts from "current" (what earlier plugins produced); in "replace" mode from
// "builtin". See PLUGIN_TARGETS.md for the {target} contract.
extern "C" int ludash_plugin_process(const char *request, char *response, size_t capacity) {{
  using Json = nlohmann::json;
  const auto input = request ? Json::parse(request, nullptr, false) : Json{{}};
  if (!input.is_object())
    return ludash_plugin_write_json("{{}}", response, capacity);
  const bool replace =
      input.contains("mode") && input["mode"].is_string() && input["mode"] == "replace";
  auto output = input.value(replace ? "builtin" : "current", Json::object());
  const auto settings = input.value("settings", Json::object());
  if (!output.is_object() || !settings.is_object())
    return ludash_plugin_write_json("{{}}", response, capacity);
{body}
  const auto bytes = output.dump();
  return ludash_plugin_write_json(bytes.c_str(), response, capacity);
}}
}} // namespace LunaDash
"""

CPP_BODIES = {
    "window-animation": """
  // The settings map one to one onto the animation profile.
  for (const auto *key : {"duration", "enterOffset", "focusOpacity", "exitScale", "easing"})
    if (settings.contains(key))
      output[key] = settings[key];
""",
}
CPP_DEFAULT_BODY = """
  // Customize the result here, for example from `settings`.
"""

C_SOURCE = """#include "core/plugins/PluginApi.h"

/* A C plugin may parse the request with any JSON library. This one returns a complete window
   animation profile, which works for both replace and augment mode. */
int ludash_plugin_process(const char *request, char *response, size_t capacity) {
    (void)request;
    return ludash_plugin_write_json(
        "{\\"duration\\":280,\\"enterOffset\\":20,\\"focusOpacity\\":0.9,"
        "\\"exitScale\\":0.95,\\"easing\\":\\"outQuint\\"}", response, capacity);
}
"""


def library_name(plugin_id):
    return re.sub(r"[^A-Za-z0-9_-]", "-", plugin_id.rsplit(".", 1)[-1]) or "plugin"


def scaffold(target, plugin_id, language="cpp"):
    """The files of a new plugin project, as {file name: contents}."""
    if language == "c" and target != "window-animation":
        raise ValueError("C scaffolds exist for the window-animation target only")
    name = library_name(plugin_id)
    source = "Effect.c" if language == "c" else "Effect.cpp"
    files = {
        "CMakeLists.txt": CMAKE.format(
            target=name,
            source=source,
            json_line="" if language == "c" else "lunadash_require_nlohmann_json()\n",
            link_line="" if language == "c" else
            f"target_link_libraries({name} PRIVATE nlohmann_json::nlohmann_json)\n"),
        source: C_SOURCE if language == "c" else CPP_SOURCE.format(
            target=target, body=CPP_BODIES.get(target, CPP_DEFAULT_BODY)),
    }
    manifest = {
        "schemaVersion": 2,
        "sdk": {"name": "LunaDash", "apiVersion": 2},
        "id": plugin_id,
        "name": f"{target} plugin",
        "version": VERSION,
        "description": f"Customizes the LunaDash {target} feature.",
        "author": "Your name",
        "type": "effect",
        "target": target,
        "mode": "augment",
        "enabledByDefault": False,
        "settings": {} if language == "c" else TARGET_SETTINGS.get(target, {}),
        "entry": f"lib{name}.so",
    }
    files["metadata.json"] = json.dumps(manifest, indent=2) + "\n"
    return files


def registry_path():
    source = Path(__file__).resolve().parents[1]
    for candidate in (source / "data/plugins/targets.json",
                      source / "share/lunadash/plugin-sdk/targets.json"):
        if candidate.is_file():
            return candidate
    raise SystemExit("targets.json was not found next to this script")


def main():
    targets = [entry for entry in json.loads(registry_path().read_text())
               if "effect" in entry["types"]]
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--list", action="store_true", help="list the native plugin targets")
    parser.add_argument("--type", choices=["effect"], default="effect",
                        help="plugin type (native effects only)")
    parser.add_argument("--target", help="the compositor feature, see --list")
    parser.add_argument("--language", choices=["cpp", "c"], default="cpp")
    parser.add_argument("--id", help="reverse-domain plugin identifier, e.g. org.example.motion")
    parser.add_argument("--output", type=Path, help="new project directory")
    args = parser.parse_args()
    if args.list:
        for target in targets:
            print(f'{target["category"]:16} {target["id"]}')
        return 0
    if not any(entry["id"] == args.target for entry in targets):
        parser.error("choose a native target from --list")
    if not args.id or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]+", args.id):
        parser.error("--id must be a reverse-domain plugin identifier")
    if not args.output or args.output.exists():
        parser.error("--output must name a new directory")
    try:
        files = scaffold(args.target, args.id, args.language)
    except ValueError as error:
        parser.error(str(error))
    args.output.mkdir(parents=True)
    for name, contents in files.items():
        (args.output / name).write_text(contents)
    print(f"Created {args.id} ({args.language} effect / {args.target}) in {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
