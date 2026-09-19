#!/usr/bin/env python3
"""Validate LunaDash's source domains, includes and explicit build inventory."""
from pathlib import Path
import argparse
import re

DOMAINS = {"compositor", "config", "core", "ctl", "desktop", "service", "shell"}
CPP_NAME = re.compile(r"[A-Z][A-Za-z0-9]*\Z")
DIRECTORY = re.compile(r"[a-z][a-z0-9]*\Z")
LEGACY = re.compile(r"\b(?:RenderBackend|RenderingBackend|RendererBackend|GraphicsBackendManager|RenderBackendManager)\w*\b")
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"\n]+)[>"]', re.M)
GL_IMPLEMENTATION = re.compile(
    r'#\s*include\s*[<"](?:GL/|GLES\w*/|QOpenGL\w*)|'
    r'\bgl[A-Z]\w*\s*\(|\bgl_?\s*(?:\.|->)\s*[A-Z]\w*\s*\('
)
SHADER_SUFFIXES = {".vert", ".frag", ".geom", ".comp", ".tesc", ".tese", ".glsl", ".glal", ".shader", ".vsh", ".fsh", ".gsh", ".csh", ".vs", ".fs", ".gs", ".cs"}
MAINS = {"compositor/Main.cpp", "desktop/Main.cpp", "ctl/Main.cpp", "shell/Main.cpp", "service/portal/Main.cpp"}


def violations(root: Path) -> list[str]:
    errors = []
    source = root / "src"
    cmake_files = [root / "CMakeLists.txt", *sorted((root / "cmake").rglob("*.cmake"))]
    cmake = "\n".join(p.read_text() for p in cmake_files if p.is_file())
    inventory = set(re.findall(r'\bsrc/[A-Za-z0-9_./-]+\.(?:cpp|c|hpp|h|vert|frag|geom|comp|tesc|tese|glsl|glal|shader)\b', cmake))
    for name in inventory:
        if not (root / name).is_file():
            errors.append(f"cmake-source-check: missing explicit source {name}")
    if not source.is_dir():
        return ["source-layout: src/ is missing"]
    for domain in DOMAINS:
        if not (source / domain).is_dir():
            errors.append(f"source-layout: missing domain src/{domain}")
    for path in sorted(source.rglob("*")):
        relative = path.relative_to(source)
        label = f"src/{relative}"
        if path.is_dir():
            if not DIRECTORY.fullmatch(path.name):
                errors.append(f"source-layout: directory must be lowercase: {label}")
            if relative.parts[0] not in DOMAINS:
                errors.append(f"source-layout: unrecognized domain: {label}")
            continue
        if path.suffix in {".cpp", ".hpp"} and not CPP_NAME.fullmatch(path.stem):
            errors.append(f"source-naming: C++ filename must use PascalCase: {label}")
        if path.suffix == ".h" and not path.with_suffix(".c").exists():
            errors.append(f"source-naming: C++ headers must use .hpp: {label}")
        if path.suffix in {".cpp", ".c"} and label not in inventory:
            errors.append(f"cmake-source-check: implementation is not explicitly listed: {label}")
        if path.suffix in SHADER_SUFFIXES:
            if relative.parts[:4] != ("compositor", "renderer", "opengl", "shaders"):
                errors.append(f"renderer-architecture: built-in shader outside OpenGL resources: {label}")
            if label not in inventory:
                errors.append(f"cmake-source-check: shader is not explicitly embedded: {label}")
        if path.suffix not in {".c", ".cpp", ".h", ".hpp"}:
            continue
        text = path.read_text(encoding="utf-8")
        if LEGACY.search(text) or LEGACY.search(path.name):
            errors.append(f"source-naming: legacy renderer naming is prohibited: {label}")
        if re.search(r'\bnamespace\s+LuDash\b|\bLuDash::', text):
            errors.append(f"source-naming: project namespace must be LunaDash: {label}")
        opengl = relative.parts[:3] == ("compositor", "renderer", "opengl")
        renderer = relative.parts[:2] == ("compositor", "renderer")
        if not opengl and GL_IMPLEMENTATION.search(text):
            errors.append(f"renderer-architecture: OpenGL implementation outside renderer/opengl: {label}")
        if re.search(r'\bint\s+main\s*\(', text) and str(relative) not in MAINS:
            errors.append(f"source-layout: entrypoint must use domain-local Main.cpp: {label}")
        for included in INCLUDE.findall(text):
            target = Path(included)
            if ".." in target.parts:
                errors.append(f"forbidden-includes: use source-root project paths: {label}: {included}")
            if target.parts[0] not in DOMAINS:
                continue
            if not (source / target).is_file():
                errors.append(f"forbidden-includes: missing project header: {label}: {included}")
            if relative.parts[0] == "core" and target.parts[0] != "core":
                errors.append(f"forbidden-includes: core cannot depend on higher domains: {label}: {included}")
            if relative.parts[0] == "config" and target.parts[0] not in {"core", "config"}:
                errors.append(f"forbidden-includes: config cannot depend on runtime domains: {label}: {included}")
            if relative.parts[0] == "desktop" and target.parts[0] == "compositor":
                errors.append(f"forbidden-includes: desktop cannot depend on compositor internals: {label}: {included}")
            if renderer and target.parts[0] == "desktop":
                errors.append(f"forbidden-includes: renderer cannot depend on desktop UI: {label}: {included}")
    for name in MAINS:
        path = source / name
        if not path.is_file() or len(path.read_text().splitlines()) > 20:
            errors.append(f"source-layout: missing or oversized entrypoint src/{name}")
    if (source / "compositor/render").exists():
        errors.append("renderer-architecture: duplicate render/renderer trees")
    if (root / "data/shaders").exists():
        errors.append("renderer-architecture: built-in shaders belong to renderer/opengl/shaders")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    errors = violations(args.root)
    for error in errors:
        print(f"error: {error}")
    if errors:
        return 1
    print("Source layout, naming, dependencies and CMake inventory passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
