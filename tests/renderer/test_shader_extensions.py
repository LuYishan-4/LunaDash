"""Check LunaDash OpenGL shader suffix discovery and stage aliases."""

from pathlib import Path

root = Path(__file__).resolve().parents[2]
cmake = (root / "cmake" / "LunaDashMain.cmake").read_text()
backend = (
    root / "src" / "compositor" / "render" / "RenderBackend" / "RenderBackend.cpp"
).read_text()

expected = {
    "vert", "frag", "geom", "comp", "tesc", "tese",
    "vsh", "fsh", "gsh", "csh", "vs", "fs", "gs", "cs", "glsl", "shader",
}

section = cmake.split("set(LUDASH_OPENGL_SHADER_EXTENSIONS", 1)[1].split(")", 1)[0]
missing_resources = sorted(ext for ext in expected if ext not in section.split())
assert not missing_resources, f"Missing shader resource suffixes: {missing_resources}"

for token in (
    '"vert"', '"vertex"', '"vsh"', '"vs"',
    '"frag"', '"fragment"', '"fsh"', '"fs"',
    '"geom"', '"geometry"', '"gsh"', '"gs"',
    '"comp"', '"compute"', '"csh"', '"cs"',
    '"tesc"', '"tese"', '"glsl"', '"shader"',
):
    assert token in backend, f"Shader stage handling is missing {token}"

assets = list((root / "data" / "shaders").rglob("*"))
shader_assets = [p for p in assets if p.is_file()]
assert shader_assets, "No shader assets found"
for path in shader_assets:
    suffix = path.suffix.lstrip(".").lower()
    assert suffix in expected, f"Unrecognized shader asset suffix: {path}"

print(
    "Shader extension checks passed:",
    ", ".join(sorted(expected)),
)
