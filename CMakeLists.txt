cmake_minimum_required(VERSION 3.21)
project(LuDash VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include(GNUInstallDirs)
option(LUDASH_ENABLE_SANITIZERS "Enable ASan and UBSan for project code" OFF)
option(LUDASH_BUILD_EXAMPLE_PLUGIN "Build the metadata-based fade effect example" ON)
if(LUDASH_ENABLE_SANITIZERS)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(FATAL_ERROR "Sanitizers require GCC or Clang")
  endif()
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
  add_link_options(-fsanitize=address,undefined)
endif()
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Quick OpenGL Network DBus WaylandCompositor)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server)
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

add_library(ludash-localization src/localization/Localization.cpp)
target_include_directories(ludash-localization PUBLIC include)
target_link_libraries(ludash-localization PUBLIC Qt6::Core)
qt_add_resources(ludash-localization translations PREFIX /LuDash FILES data/translations/en_US.json data/translations/zh_TW.json)
add_library(ludash-renderer src/renderer/RenderBackend.cpp src/renderer/WallpaperItem.cpp src/renderer/WallpaperRenderer.cpp)
target_include_directories(ludash-renderer PUBLIC include)
target_link_libraries(ludash-renderer PUBLIC Qt6::Quick Qt6::OpenGL)
qt_add_resources(ludash-renderer renderer_shaders PREFIX /LuDash FILES data/shaders/wallpaper.vert data/shaders/wallpaper.frag)
add_library(ludash-wallpaper src/wallpaper/WallpaperSettings.cpp)
target_include_directories(ludash-wallpaper PUBLIC include)
target_link_libraries(ludash-wallpaper PUBLIC Qt6::Gui)
target_compile_definitions(ludash-wallpaper PRIVATE LUDASH_WALLPAPER_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/wallpapers")
add_library(ludash-configuration src/configuration/DesktopPreferences.cpp)
target_include_directories(ludash-configuration PUBLIC include)
target_link_libraries(ludash-configuration PUBLIC Qt6::Core)
add_library(ludash-network src/network/NetworkStatus.cpp)
target_include_directories(ludash-network PUBLIC include)
target_link_libraries(ludash-network PUBLIC Qt6::Network Qt6::DBus)
add_library(ludash-tiling src/tiling/TilingLayout.cpp)
target_include_directories(ludash-tiling PUBLIC include)
target_link_libraries(ludash-tiling PUBLIC Qt6::Core)
add_library(ludash-plugins src/plugins/PluginManager.cpp)
target_include_directories(ludash-plugins PUBLIC include)
target_link_libraries(ludash-plugins PUBLIC Qt6::Quick)
add_library(ludash-apps
    src/theme/DesktopTheme.cpp
    src/application_catalog/ApplicationCatalog.cpp
    src/application_window/ApplicationWindow.cpp
    src/file_manager/FileManager.cpp
    src/console/Console.cpp
    src/notes/Notes.cpp
    src/system_monitor/SystemMonitor.cpp
    src/welcome/Welcome.cpp
    src/settings/Settings.cpp
    src/launcher/Launcher.cpp
    src/packages/PackageManager.cpp
    src/plugin_settings/PluginSettings.cpp
)
target_include_directories(ludash-apps PUBLIC include)
target_link_libraries(ludash-apps PUBLIC ludash-localization ludash-plugins ludash-wallpaper Qt6::Widgets)
target_compile_options(ludash-apps PRIVATE -Wall -Wextra -Wpedantic)

set(LAYER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/wlr-layer-shell-server.h)
set(LAYER_CODE ${CMAKE_CURRENT_BINARY_DIR}/wlr-layer-shell-protocol.c)
add_custom_command(OUTPUT ${LAYER_HEADER} ${LAYER_CODE}
    COMMAND ${WAYLAND_SCANNER} server-header ${CMAKE_CURRENT_SOURCE_DIR}/protocols/wlr-layer-shell-unstable-v1.xml ${LAYER_HEADER}
    COMMAND ${WAYLAND_SCANNER} private-code ${CMAKE_CURRENT_SOURCE_DIR}/protocols/wlr-layer-shell-unstable-v1.xml ${LAYER_CODE}
    DEPENDS protocols/wlr-layer-shell-unstable-v1.xml VERBATIM)
add_library(ludash-wayland
    src/compositor/WaylandCompositor.cpp
    src/window_frame/WindowFrame.cpp
    src/input_method/InputMethodSupport.cpp
    src/layer_shell/LayerShell.cpp
    src/layer_shell/LayerSurface.cpp
    src/ipc/ControlServer.cpp
    src/system_status/SystemStatus.cpp
    ${LAYER_HEADER} ${LAYER_CODE}
)
target_include_directories(ludash-wayland PUBLIC include PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(ludash-wayland PRIVATE LUDASH_QML_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
target_link_libraries(ludash-wayland PUBLIC ludash-configuration ludash-network ludash-wallpaper ludash-renderer ludash-tiling ludash-localization ludash-plugins Qt6::Quick Qt6::Network Qt6::WaylandCompositor PkgConfig::WAYLAND_SERVER)
target_compile_options(ludash-wayland PRIVATE -Wall -Wextra -Wpedantic)
add_executable(ludash-desktop src/entrypoints/desktop_main.cpp)
target_link_libraries(ludash-desktop PRIVATE ludash-apps)
add_executable(ludash-compositor src/entrypoints/compositor_main.cpp)
target_link_libraries(ludash-compositor PRIVATE ludash-wayland)
add_executable(ludashctl src/entrypoints/control_main.cpp)
target_link_libraries(ludashctl PRIVATE Qt6::Network)

if(LUDASH_BUILD_EXAMPLE_PLUGIN)
  add_library(ludash-fade MODULE include/LuDash/fade_plugin/FadePlugin.h src/fade_plugin/FadePlugin.cpp)
  target_include_directories(ludash-fade PRIVATE include)
  target_link_libraries(ludash-fade PRIVATE Qt6::Quick)
  set_target_properties(ludash-fade PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/org.ludash.fade)
  configure_file(data/plugins/fade/metadata.json plugins/org.ludash.fade/metadata.json COPYONLY)
  install(TARGETS ludash-fade LIBRARY DESTINATION ${CMAKE_INSTALL_DATADIR}/ludash/plugins/org.ludash.fade)
  install(FILES data/plugins/fade/metadata.json DESTINATION ${CMAKE_INSTALL_DATADIR}/ludash/plugins/org.ludash.fade)
endif()
configure_file(data/ludash.desktop.in ludash.desktop @ONLY)
install(TARGETS ludash-desktop ludash-compositor ludashctl RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS scripts/ludash-session DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ludash.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/wayland-sessions)
install(DIRECTORY qml/ DESTINATION ${CMAKE_INSTALL_DATADIR}/ludash/shell)
install(DIRECTORY data/wallpapers/ DESTINATION ${CMAKE_INSTALL_DATADIR}/ludash/wallpapers)
install(DIRECTORY data/translations/ DESTINATION ${CMAKE_INSTALL_DATADIR}/ludash/translations)
install(FILES include/LuDash/plugins/CompositorPlugin.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/LuDash/plugins)
include(CTest)
if(BUILD_TESTING)
  find_package(Qt6 REQUIRED COMPONENTS Test)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_executable(ludash-tests tests/DesktopTests.cpp)
  target_link_libraries(ludash-tests PRIVATE ludash-apps ludash-tiling Qt6::Test)
  add_test(NAME desktop-interactions COMMAND ludash-tests)
  set_tests_properties(desktop-interactions PROPERTIES TIMEOUT 45)
  add_test(NAME security-gate COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/security/test_sarif_gate.py)
  add_executable(ludash-render-tests tests/renderer/RenderTests.cpp)
  target_link_libraries(ludash-render-tests PRIVATE ludash-renderer Qt6::Test)
  add_test(NAME graphics-contexts COMMAND ${CMAKE_COMMAND} -E env --unset=MESA_GL_VERSION_OVERRIDE --unset=MESA_GLSL_VERSION_OVERRIDE $<TARGET_FILE:ludash-render-tests>)
  set_tests_properties(graphics-contexts PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_XCB_GL_INTEGRATION=xcb_egl")
  add_test(NAME graphics-startup-failure COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/renderer/test_startup_failure.py $<TARGET_FILE:ludash-compositor>)
  set_tests_properties(graphics-startup-failure PROPERTIES TIMEOUT 30)
  add_executable(ludash-preference-tests tests/configuration/PreferenceTests.cpp)
  target_link_libraries(ludash-preference-tests PRIVATE ludash-configuration ludash-network Qt6::Test)
  add_test(NAME desktop-preferences COMMAND ludash-preference-tests)
  add_test(NAME source-language COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/security/test_source_language.py)
endif()
