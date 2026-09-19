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
    src/compositor/animation/WindowAnimations.cpp
    src/compositor/animation/SceneWindowAnimations.cpp)
target_include_directories(ludash-animation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(ludash-animation PRIVATE WLR_USE_UNSTABLE=1)
target_link_libraries(ludash-animation
    PUBLIC Qt6::Quick
    PRIVATE PkgConfig::WLROOTS)

add_library(ludash-shell-renderer
    src/shell/runtime/ShellRenderer.cpp)
target_include_directories(ludash-shell-renderer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ludash-shell-renderer PUBLIC Qt6::Core)

