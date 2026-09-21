"""Validate the renderer's public boundary, resources and build targets."""

import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[2]
subprocess.run(
    [sys.executable, str(root / "scripts/check-source-layout.py")], check=True
)
renderer = root / "src/compositor/renderer"
for name in (
    "Renderer.cpp",
    "Renderer.hpp",
    "RendererConfig.hpp",
    "RendererTypes.hpp",
    "opengl/OpenGL.cpp",
    "opengl/GLDispatch.c",
    "opengl/Shader.cpp",
    "opengl/Program.cpp",
    "opengl/Texture.cpp",
    "opengl/Framebuffer.cpp",
    "opengl/ShaderAsset.cpp",
    "element/ElementRender.hpp",
    "async/RenderAsync.hpp",
    "blur/BlurItem.cpp",
    "blur/BlurGeometry.cpp",
    "opengl/blur/BlurPass.cpp",
    "opengl/blur/BlurNode.cpp",
    "opengl/wallpaper/WallpaperElement.cpp",
    "opengl/decoration/DecorationElement.cpp",
):
    assert (renderer / name).is_file(), name
for name in ("Feature.hpp", "Module.hpp", "Renderer.hpp", "WaylandSlot.hpp", "WindowAnimation.hpp"):
    assert (root / "src/core/templates" / name).is_file(), name
runtime = (root / "src/compositor/wayland/Register.hpp").read_text()
assert '"core/templates/WaylandSlot.hpp"' in runtime
assert "template <typename Owner> struct ListenerSlot" not in runtime
cmake = (root / "cmake/modules/Renderer.cmake").read_text()
for target in (
    "ludash-render-gl",
    "ludash-render-shader",
    "ludash-render-core",
    "ludash-renderer",
    "ludash-blur",
    "ludash-animation",
):
    assert target in cmake, target
assert "src/compositor/window/animation/SceneAnimationBackend.cpp" in cmake
assert (root / "src/compositor/window/animation/WindowAnimation.hpp").is_file()
assert (root / "src/compositor/window/animation/SceneAnimationBackend.hpp").is_file()
assert "PREFIX /LunaDash/renderer/shaders" in cmake
assert "LUDASH_RENDERER_OPENGL=1" in cmake
assert "file(GLOB" not in cmake, "Renderer sources/resources must be explicit"
print("Renderer boundaries and embedded resource build passed")
