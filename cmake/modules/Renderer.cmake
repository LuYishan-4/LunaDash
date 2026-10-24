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
target_include_directories(ludash-render-selection PUBLIC src)
target_compile_definitions(ludash-render-selection PRIVATE WLR_USE_UNSTABLE=1
    LUDASH_HAS_VULKAN_RENDERER=$<AND:$<BOOL:${LUDASH_ENABLE_VULKAN}>,$<BOOL:${LUDASH_WLROOTS_VULKAN}>>)
target_link_libraries(ludash-render-selection PUBLIC PkgConfig::WLROOTS)
if(LUDASH_VULKAN_FOUND)
    target_link_libraries(ludash-render-selection PRIVATE PkgConfig::LUDASH_VULKAN)
endif()
message(STATUS "LunaDash Vulkan: enabled=${LUDASH_ENABLE_VULKAN}, wlroots=${LUDASH_WLROOTS_VULKAN}")

# Toolkit-neutral OpenGL utilities used by LunaDash-owned render elements.
# The compositor's client scene remains owned by wlroots; this API only runs
# when an EGL OpenGL context is current in the calling process.
pkg_check_modules(LUDASH_EGL REQUIRED IMPORTED_TARGET egl)

set(LUDASH_RENDER_SHADER_FILES
    src/compositor/renderer/opengl/shaders/Fullscreen.vert
    src/compositor/renderer/opengl/shaders/Wallpaper.frag
    src/compositor/renderer/opengl/shaders/Blur.frag
    src/compositor/renderer/opengl/shaders/Decoration.frag)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${LUDASH_RENDER_SHADER_FILES})
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/compositor/renderer/opengl/shaders/Fullscreen.vert"
    LUDASH_SHADER_FULLSCREEN_VERT)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/compositor/renderer/opengl/shaders/Wallpaper.frag"
    LUDASH_SHADER_WALLPAPER_FRAG)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/compositor/renderer/opengl/shaders/Blur.frag"
    LUDASH_SHADER_BLUR_FRAG)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/compositor/renderer/opengl/shaders/Decoration.frag"
    LUDASH_SHADER_DECORATION_FRAG)
set(LUDASH_RENDER_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/renderer")
file(MAKE_DIRECTORY "${LUDASH_RENDER_GENERATED_DIR}")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ShaderResources.hpp.in
    ${LUDASH_RENDER_GENERATED_DIR}/ShaderResources.hpp @ONLY)

add_library(ludash-render-gl
    src/compositor/renderer/opengl/GLDispatch.c
    src/compositor/renderer/opengl/OpenGL.cpp
    src/compositor/renderer/opengl/Texture.cpp
    src/compositor/renderer/opengl/Framebuffer.cpp)
target_include_directories(ludash-render-gl
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${LUDASH_GL_INCLUDE_DIR})
target_link_libraries(ludash-render-gl
    PUBLIC PkgConfig::LUDASH_EGL)
target_compile_definitions(ludash-render-gl PUBLIC LUDASH_RENDERER_OPENGL=1)
target_compile_options(ludash-render-gl PRIVATE -Wall -Wextra -Wpedantic)

add_library(ludash-render-shader
    src/compositor/renderer/opengl/ShaderAsset.cpp
    src/compositor/renderer/opengl/Program.cpp
    src/compositor/renderer/opengl/Shader.cpp)
target_include_directories(ludash-render-shader
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${LUDASH_RENDER_GENERATED_DIR})
target_link_libraries(ludash-render-shader
    PUBLIC ludash-render-gl)

add_library(ludash-render-core
    src/compositor/renderer/element/ElementRender.hpp
    src/compositor/renderer/Renderer.cpp
    src/compositor/renderer/opengl/wallpaper/WallpaperElement.cpp
    src/compositor/renderer/opengl/decoration/DecorationElement.cpp)
target_include_directories(ludash-render-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ludash-render-core
    PUBLIC ludash-render-shader)
target_compile_options(ludash-render-core PRIVATE -Wall -Wextra -Wpedantic)

add_library(ludash-renderer INTERFACE)
target_include_directories(ludash-renderer INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ludash-renderer INTERFACE ludash-render-core)

add_library(ludash-blur
    src/compositor/renderer/opengl/blur/BlurPass.cpp)
target_include_directories(ludash-blur PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ludash-blur PUBLIC ludash-renderer)

add_library(ludash-animation
    src/compositor/window/animation/SceneAnimation.cpp)
target_include_directories(ludash-animation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(ludash-animation PRIVATE WLR_USE_UNSTABLE=1)
target_link_libraries(ludash-animation
    PRIVATE PkgConfig::WLROOTS PkgConfig::WAYLAND_SERVER)

add_library(ludash-shell-renderer
    src/shell/runtime/ShellRenderer.cpp)
target_include_directories(ludash-shell-renderer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
