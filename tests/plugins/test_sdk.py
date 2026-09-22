#!/usr/bin/env python3
"""Build and relocate SDK templates and real plugin packages."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


def run(*command, ok=True):
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if (result.returncode == 0) != ok:
        raise AssertionError(f'{" ".join(map(str, command))}\n{result.stdout}')
    return result


def build_and_install(project, build, sdk, prefix):
    run("cmake", "-S", str(project), "-B", str(build), "-G", "Ninja",
        f"-DLunaDashPlugin_DIR={sdk.resolve()}", f"-DCMAKE_INSTALL_PREFIX={prefix}")
    run("cmake", "--build", str(build), "--parallel", "2")
    run("cmake", "--install", str(build))
    manifest = json.loads((project / "metadata.json").read_text())
    installed = prefix / "share/lunadash/plugins" / manifest["id"]
    assert (installed / ".lunadash-sdk.json").is_file()
    return manifest, installed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, required=True)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[2]
    with tempfile.TemporaryDirectory(prefix="lunadash-sdk-test-") as directory:
        root = Path(directory)
        prefix = root / "install"
        for name in ("effect-c", "effect-cpp", "window-animation", "quickshell", "opengl"):
            project = root / name
            shutil.copytree(source / "templates/plugins" / name, project)
            manifest, installed = build_and_install(
                project, root / (name + "-build"), args.sdk, prefix)
            if name == "opengl":
                assert (installed / "Effect.vert.qsb").stat().st_size > 0
                assert (installed / "Effect.frag.qsb").stat().st_size > 0
            else:
                assert (installed / manifest["entry"]).is_file()
            print(f"SDK template built and staged: {name}")

        for name in ("fade", "stacking-windows"):
            project = root / name
            shutil.copytree(source / "data/plugins" / name, project)
            manifest, installed = build_and_install(
                project, root / (name + "-build"), args.sdk, prefix)
            assert (installed / manifest["entry"]).is_file()
            print(f"SDK 2 plugin package built and staged: {name}")

        broken = root / "quickshell/metadata.json"
        manifest = json.loads(broken.read_text())
        manifest["target"] = "window-layout" # Quickshell cannot own native geometry.
        broken.write_text(json.dumps(manifest))
        run("cmake", "-S", str(broken.parent), "-B", str(root / "invalid"),
            f"-DLunaDashPlugin_DIR={args.sdk.resolve()}", ok=False)
        print("SDK rejects an incompatible target/type pair")

        text_control = root / "effect-cpp/metadata.json"
        manifest = json.loads(text_control.read_text())
        manifest["settings"]["freeText"] = {
            "type": "string", "default": "custom", "control": "text"
        }
        text_control.write_text(json.dumps(manifest))
        run("cmake", "-S", str(text_control.parent),
            "-B", str(root / "invalid-text-control"),
            f"-DLunaDashPlugin_DIR={args.sdk.resolve()}", ok=False)
        print("SDK rejects plugin settings that require custom text controls")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
