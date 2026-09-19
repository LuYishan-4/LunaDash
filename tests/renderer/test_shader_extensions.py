"""Validate the modular shader asset pipeline and supported suffixes."""

from pathlib import Path

root = Path(__file__).resolve().parents[2]
cmake = (root / "cmake/modules/Renderer.cmake").read_text(encoding="utf-8")
asset_loader = (
    root / "src/compositor/renderer/opengl/ShaderAsset.cpp"
).read_text(encoding="utf-8")
renderer = (
    root / "src/compositor/renderer/Renderer.cpp"
).read_text(encoding="utf-8")
wallpaper = (
    root / "src/compositor/renderer/opengl/wallpaper/WallpaperElement.cpp"
).read_text(encoding="utf-8")
blur = (
    root / "src/compositor/renderer/opengl/blur/BlurPass.cpp"
).read_text(encoding="utf-8")

expected = {
    "vert", "frag", "geom", "comp", "tesc", "tese",
    "vsh", "fsh", "gsh", "csh", "vs", "fs", "gs", "cs",
    "glsl", "glal", "shader",
}

aliases = {
    "vertex": ("vert", "vertex", "vsh", "vs"),
    "fragment": ("frag", "fragment", "fsh", "fs"),
    "geometry": ("geom", "geometry", "gsh", "gs"),
    "compute": ("comp", "compute", "csh", "cs"),
    "tess-control": ("tesc", "tesscontrol", "tess_control"),
    "tess-evaluation": ("tese", "tesseval", "tess_eval"),
}
for stage, tokens in aliases.items():
    missing = [token for token in tokens if f'"{token}"' not in asset_loader]
    assert not missing, f"{stage} aliases missing from ShaderAssetLoader: {missing}"

for generic in ('"glsl"', '"glal"', '"shader"', "#pragma ludash_stage"):
    assert generic in asset_loader, f"Generic shader handling is missing {generic}"

shader_root = root / "src/compositor/renderer/opengl/shaders"
assets = [path for path in shader_root.rglob("*") if path.is_file()]
assert assets, "No shader assets found"
for path in assets:
    suffix = path.suffix.lstrip(".").lower()
    assert str(path.relative_to(root)) in cmake, f"Shader missing from CMake: {path}"
    assert suffix in expected, f"Unrecognized shader asset suffix: {path}"

assert (shader_root / "Fullscreen.vert").is_file()
assert (shader_root / "Wallpaper.frag").is_file()
assert (shader_root / "Blur.frag").is_file()
assert (shader_root / "Decoration.frag").is_file()

assert "ShaderAssetLoader::loadMany" in renderer
assert "Wallpaper.frag" in wallpaper
assert "Fullscreen.vert" in wallpaper
assert "Blur.frag" in blur
assert "Fullscreen.vert" in blur

for path in (root / "src/compositor/renderer").rglob("*"):
    if path.suffix not in {".c", ".cpp", ".h", ".hpp"}:
        continue
    text = path.read_text(encoding="utf-8")
    for marker in ("gl_Position =", "uniform sampler2D", "void main() {"):
        assert marker not in text, f"Embedded GLSL marker {marker!r} found in {path}"

print("Shader asset checks passed:", ", ".join(sorted(expected)))
