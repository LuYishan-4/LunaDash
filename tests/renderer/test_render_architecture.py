"""Keep the render tree modular and extension-oriented."""

from pathlib import Path

root = Path(__file__).resolve().parents[2]
render = root / "src" / "compositor" / "render"

required = {
    "renderer": [
        "Renderer.hpp", "Renderer.cpp", "WallpaperElement.hpp",
        "WallpaperElement.cpp", "WallpaperRenderer.hpp", "WallpaperRenderer.cpp",
    ],
    "gl": [
        "GLContext.hpp", "GLContext.cpp", "GLDispatch.h", "GLDispatch.c",
        "RenderBuffer.hpp", "RenderBuffer.cpp", "FrameBuffer.hpp", "FrameBuffer.cpp",
    ],
    "shader": [
        "ShaderAsset.hpp", "ShaderAsset.cpp", "ShaderProgram.hpp", "ShaderProgram.cpp",
    ],
    "element": ["ElementRender.hpp"],
    "async": ["RenderAsync.hpp"],
    "blur": [
        "BlurPass.hpp", "BlurPass.cpp", "BlurNode.hpp", "BlurNode.cpp",
        "BlurItem.hpp", "BlurItem.cpp", "BlurGeometry.hpp", "BlurGeometry.cpp",
    ],
    "decorations": [
        "DecorationElement.hpp", "DecorationElement.cpp",
        "WindowAnimations.hpp", "WindowAnimations.cpp",
    ],
}

for folder, files in required.items():
    for name in files:
        path = render / folder / name
        assert path.is_file(), f"Missing render module: {path.relative_to(root)}"

legacy = [
    "RenderBackend", "GLDispatch", "ShaderProgram", "BlurPass", "BlurNode",
    "BlurItem", "BlurGeometry", "WallpaperRenderer", "WallpaperItem",
    "WallpaperPass", "ShellRenderer", "WindowAnimations",
]
for folder in legacy:
    assert not (render / folder).exists(), f"Legacy render folder still exists: {folder}"

element = (render / "element" / "ElementRender.hpp").read_text(encoding="utf-8")
assert "template <typename Element" in element
assert "std::derived_from<Element, ElementRender>" in element

wayland_template = root / "src" / "core" / "templates" / "WaylandListener.hpp"
assert wayland_template.is_file(), "Reusable Wayland listener template is missing"
wayland = (
    root / "src" / "compositor" / "WaylandCompositor" / "WaylandCompositor.cpp"
).read_text(encoding="utf-8")
assert '#include "core/templates/WaylandListener.hpp"' in wayland
assert "template <typename Owner> struct ListenerSlot" not in wayland

cmake = (root / "cmake" / "LunaDashMain.cmake").read_text(encoding="utf-8")
for target in (
    "ludash-render-gl", "ludash-render-shader", "ludash-render-core",
    "ludash-renderer", "ludash-blur",
):
    assert target in cmake, f"Missing modular render target {target}"

print("Render architecture checks passed")
