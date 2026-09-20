#!/usr/bin/env python3
"""Build and relocate real SDK templates without installing into the user's data."""
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, required=True)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[2]
    with tempfile.TemporaryDirectory(prefix="lunadash-sdk-test-") as directory:
        root = Path(directory)
        prefix = root / "install"
        for name in ("effect-c", "effect-cpp", "quickshell", "opengl"):
            project = root / name
            shutil.copytree(source / "templates/plugins" / name, project)
            build = root / (name + "-build")
            run("cmake", "-S", str(project), "-B", str(build), "-G", "Ninja",
                f"-DLunaDashPlugin_DIR={args.sdk.resolve()}", f"-DCMAKE_INSTALL_PREFIX={prefix}")
            run("cmake", "--build", str(build), "--parallel", "2")
            run("cmake", "--install", str(build))
            manifest = json.loads((project / "metadata.json").read_text())
            installed = prefix / "share/lunadash/plugins" / manifest["id"]
            assert (installed / ".lunadash-sdk.json").is_file()
            if name == "opengl":
                assert (installed / "Effect.vert.qsb").stat().st_size > 0
                assert (installed / "Effect.frag.qsb").stat().st_size > 0
            else:
                assert (installed / manifest["entry"]).is_file()
            print(f"SDK template built and staged: {name}")
        broken = root / "quickshell/metadata.json"
        manifest = json.loads(broken.read_text())
        manifest["target"] = "window-layout" # Quickshell cannot own native geometry.
        broken.write_text(json.dumps(manifest))
        run("cmake", "-S", str(broken.parent), "-B", str(root / "invalid"),
            f"-DLunaDashPlugin_DIR={args.sdk.resolve()}", ok=False)
        print("SDK rejects an incompatible target/type pair")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
