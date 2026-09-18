"""Check LunaDash OpenGL shader assets, suffix discovery, and render usage."""

from pathlib import Path

root = Path(__file__).resolve().parents[2]
cmake = (root / "cmake" / "LunaDashMain.cmake").read_text()
backend = (
    root / "src" / "compositor" / "render" / "RenderBackend" / "RenderBackend.cpp"
).read_text()
wallpaper = (
    root / "src" / "compositor" / "render" / "WallpaperRenderer" / "WallpaperRenderer.cpp"
).read_text()
blur = (
    root / "src" / "compositor" / "render" / "BlurNode" / "BlurNode.cpp"
).read_text()

expected = {
    "vert", "frag", "geom", "comp", "tesc", "tese",
    "vsh", "fsh", "gsh", "csh", "vs", "fs", "gs", "cs", "glsl", "shader",
}

section = cmake.split("set(LUDASH_OPENGL_SHADER_EXTENSIONS", 1)[1].split(")", 1)[0]
missing_resources = sorted(ext for ext in expected if ext not in section.split())
assert not missing_resources, f"Missing shader resource suffixes: {missing_resources}"

aliases = {
    "vertex": ("vert", "vertex", "vsh", "vs"),
    "fragment": ("frag", "fragment", "fsh", "fs"),
    "geometry": ("geom", "geometry", "gsh", "gs"),
    "compute": ("comp", "compute", "csh", "cs"),
    "tess-control": ("tesc", "tesscontrol", "tess_control"),
    "tess-evaluation": ("tese", "tesseval", "tess_eval"),
}
for stage, tokens in aliases.items():
    missing = [token for token in tokens if f'"{token}"' not in backend]
    assert not missing, f"{stage} aliases missing from loader: {missing}"

for generic in ('"glsl"', '"shader"', "#pragma ludash_stage"):
    assert generic in backend, f"Generic shader handling is missing {generic}"

shader_root = root / "data" / "shaders"
assets = [path for path in shader_root.rglob("*") if path.is_file()]
assert assets, "No shader assets found"
for path in assets:
    suffix = path.suffix.lstrip(".").lower()
    assert suffix in expected, f"Unrecognized shader asset suffix: {path}"

fullscreen = shader_root / "common" / "fullscreen.vert"
assert fullscreen.is_file(), "Shared fullscreen vertex shader is missing"
assert not (shader_root / "wallpaper.vert").exists(), "Wallpaper still has a duplicate vertex shader"
assert not (shader_root / "blur" / "blur.vert").exists(), "Blur still has a duplicate vertex shader"

for source, label in ((wallpaper, "wallpaper"), (blur, "blur")):
    assert "shaderProgramFromAssets" in source, f"{label} bypasses the shader asset loader"
    assert "common/fullscreen.vert" in source, f"{label} does not use the shared .vert asset"

for path in (root / "src" / "compositor" / "render").rglob("*"):
    if path.suffix not in {".c", ".cpp", ".h", ".hpp"}:
        continue
    text = path.read_text()
    for marker in ("gl_Position =", "uniform sampler", "void main() {"):
        assert marker not in text, f"Embedded GLSL marker {marker!r} found in {path}"

print("Shader asset checks passed:", ", ".join(sorted(expected)))
