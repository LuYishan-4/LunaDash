# Toolkit-independent wlroots renderer selection. Vulkan is optional so that
# distributions building wlroots without it retain GLES/pixman support.
option(LUDASH_ENABLE_VULKAN "Enable the wlroots Vulkan renderer when available" ON)
include(CheckCSourceCompiles)
pkg_check_modules(LUDASH_VULKAN QUIET IMPORTED_TARGET vulkan)
set(CMAKE_REQUIRED_LIBRARIES PkgConfig::WLROOTS)
if(LUDASH_VULKAN_FOUND)
    list(APPEND CMAKE_REQUIRED_LIBRARIES PkgConfig::LUDASH_VULKAN)
endif()
set(CMAKE_REQUIRED_DEFINITIONS -DWLR_USE_UNSTABLE=1)
check_c_source_compiles("
#include <wlr/config.h>
#if !WLR_HAS_VULKAN_RENDERER
#error Vulkan disabled in wlroots
#endif
#include <wlr/render/vulkan.h>
int main(void) { return wlr_renderer_is_vk(0); }
" LUDASH_WLROOTS_VULKAN)
unset(CMAKE_REQUIRED_LIBRARIES)
unset(CMAKE_REQUIRED_DEFINITIONS)
add_library(ludash-render-selection
    src/compositor/renderer/selection/RenderSelection.c)
set_target_properties(ludash-render-selection PROPERTIES AUTOMOC OFF)
target_include_directories(ludash-render-selection PUBLIC src)
target_compile_definitions(ludash-render-selection PRIVATE WLR_USE_UNSTABLE=1
    LUDASH_HAS_VULKAN_RENDERER=$<AND:$<BOOL:${LUDASH_ENABLE_VULKAN}>,$<BOOL:${LUDASH_WLROOTS_VULKAN}>>)
target_link_libraries(ludash-render-selection PUBLIC PkgConfig::WLROOTS)
if(LUDASH_VULKAN_FOUND)
    target_link_libraries(ludash-render-selection PRIVATE PkgConfig::LUDASH_VULKAN)
endif()
message(STATUS "LunaDash Vulkan: enabled=${LUDASH_ENABLE_VULKAN}, wlroots=${LUDASH_WLROOTS_VULKAN}")

# Modular render architecture. Low-level GL ownership, shader assets,
# render elements and feature passes are separate targets so future renderers
# can replace one layer without rewriting the rest of the pipeline.
add_library(ludash-render-gl
    src/compositor/renderer/opengl/GLDispatch.c
    src/compositor/renderer/opengl/OpenGL.cpp
    src/compositor/renderer/opengl/Texture.cpp
    src/compositor/renderer/opengl/Framebuffer.cpp)
target_include_directories(ludash-render-gl
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${LUDASH_GL_INCLUDE_DIR})
target_link_libraries(ludash-render-gl
    PUBLIC Qt6::Core Qt6::Quick Qt6::OpenGL)
target_compile_definitions(ludash-render-gl PUBLIC LUDASH_RENDERER_OPENGL=1)
target_compile_options(ludash-render-gl PRIVATE -Wall -Wextra -Wpedantic)

# Built-in implementation resources are explicit and embedded, never opened
# relative to the repository or process working directory.
set(LUDASH_RENDER_SHADER_FILES
    src/compositor/renderer/opengl/shaders/Fullscreen.vert
    src/compositor/renderer/opengl/shaders/Wallpaper.frag
    src/compositor/renderer/opengl/shaders/Blur.frag
    src/compositor/renderer/opengl/shaders/Decoration.frag)

add_library(ludash-render-shader
    src/compositor/renderer/opengl/ShaderAsset.cpp
    src/compositor/renderer/opengl/Program.cpp
    src/compositor/renderer/opengl/Shader.cpp)
target_include_directories(ludash-render-shader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ludash-render-shader
    PUBLIC ludash-render-gl Qt6::Core Qt6::OpenGL)
qt_add_resources(ludash-render-shader renderer_shaders
    PREFIX /LunaDash/renderer/shaders
    BASE ${CMAKE_CURRENT_SOURCE_DIR}/src/compositor/renderer/opengl/shaders
    FILES ${LUDASH_RENDER_SHADER_FILES})

add_library(ludash-render-core
    src/compositor/renderer/async/RenderAsync.hpp
    src/compositor/renderer/element/ElementRender.hpp
    src/compositor/renderer/Renderer.cpp
    src/compositor/renderer/opengl/wallpaper/WallpaperElement.cpp
    src/compositor/renderer/opengl/wallpaper/WallpaperRenderer.cpp
    src/compositor/renderer/wallpaper/WallpaperItem.cpp
    src/compositor/renderer/opengl/decoration/DecorationElement.cpp)
target_include_directories(ludash-render-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ludash-render-core
    PUBLIC ludash-render-shader Qt6::Quick Qt6::OpenGL Qt6::Concurrent)
target_compile_options(ludash-render-core PRIVATE -Wall -Wextra -Wpedantic)

add_library(ludash-renderer INTERFACE)
target_include_directories(ludash-renderer INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ludash-renderer INTERFACE ludash-render-core)

add_library(ludash-blur
    src/compositor/renderer/blur/BlurItem.cpp
    src/compositor/renderer/opengl/blur/BlurNode.cpp
    src/compositor/renderer/blur/BlurGeometry.cpp
    src/compositor/renderer/opengl/blur/BlurPass.cpp)
target_include_directories(ludash-blur PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ludash-blur
    PUBLIC ludash-renderer Qt6::Quick Qt6::OpenGL)

add_library(ludash-animation
    src/compositor/window/animation/SceneAnimation.cpp)
target_include_directories(ludash-animation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(ludash-animation PRIVATE WLR_USE_UNSTABLE=1)
target_link_libraries(ludash-animation
    PUBLIC Qt6::Core
    PRIVATE PkgConfig::WLROOTS PkgConfig::WAYLAND_SERVER)

add_library(ludash-shell-renderer
    src/shell/runtime/ShellRenderer.cpp)
target_include_directories(ludash-shell-renderer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ludash-shell-renderer PUBLIC Qt6::Core)
