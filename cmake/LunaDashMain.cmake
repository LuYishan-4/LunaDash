cmake_minimum_required(VERSION 3.21)
project(LunaDash VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
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
if(CMAKE_CXX_CLANG_TIDY AND NOT CMAKE_C_CLANG_TIDY)
    set(CMAKE_C_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY}")
endif()
find_path(LUDASH_GL_INCLUDE_DIR GL/glcorearb.h REQUIRED)
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Concurrent Quick OpenGL Network DBus)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
pkg_check_modules(LUDASH_XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)
pkg_search_module(WLROOTS REQUIRED IMPORTED_TARGET
    wlroots-0.20 wlroots-0.19 wlroots-0.18 "wlroots>=0.17")
message(STATUS "LunaDash compositor backend: wlroots ${WLROOTS_VERSION} (${WLROOTS_MODULE_NAME})")
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.lunadash-revision")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/.lunadash-revision" LUNADASH_GIT_COMMIT LIMIT_COUNT 1)
else()
    execute_process(COMMAND git rev-parse --verify HEAD WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE LUNADASH_GIT_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
if(NOT LUNADASH_GIT_COMMIT)
    set(LUNADASH_GIT_COMMIT "unknown")
endif()

add_compile_definitions(
    LUDASH_VERSION="${PROJECT_VERSION}"
    LUDASH_GIT_COMMIT="${LUNADASH_GIT_COMMIT}")

add_library(ludash-localization src/config/localization/Localization.cpp src/config/localization/JsonTranslator.cpp)
target_include_directories(ludash-localization PUBLIC src)
target_link_libraries(ludash-localization PUBLIC Qt6::Core)
qt_add_resources(ludash-localization translations PREFIX /LuDash FILES data/translations/en_US.json data/translations/zh_TW.json)
include(${CMAKE_CURRENT_LIST_DIR}/modules/Renderer.cmake)

add_library(ludash-xwayland src/compositor/xwayland/XWaylandSupport.cpp)
target_include_directories(ludash-xwayland PUBLIC src)
target_link_libraries(ludash-xwayland PUBLIC Qt6::Core)
add_library(ludash-wallpaper src/desktop/wallpaper/WallpaperSettings.cpp)
target_include_directories(ludash-wallpaper PUBLIC src)
target_link_libraries(ludash-wallpaper PUBLIC Qt6::Gui)
target_compile_definitions(ludash-wallpaper PRIVATE LUDASH_WALLPAPER_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/wallpapers")
add_library(ludash-configuration src/config/desktop/DesktopPreferences.cpp)
target_include_directories(ludash-configuration PUBLIC src)
target_link_libraries(ludash-configuration PUBLIC Qt6::Core)
add_library(ludash-process-runner src/config/command/CommandRunner.cpp)
target_include_directories(ludash-process-runner PUBLIC src)
target_link_libraries(ludash-process-runner PUBLIC Qt6::Core)
add_library(ludash-audio-settings src/desktop/audio/AudioSettings.cpp)
target_include_directories(ludash-audio-settings PUBLIC src)
target_link_libraries(ludash-audio-settings PUBLIC ludash-process-runner)
add_library(ludash-power-settings src/desktop/power/PowerSettings.cpp)
target_include_directories(ludash-power-settings PUBLIC src)
target_link_libraries(ludash-power-settings PUBLIC ludash-process-runner)
add_library(ludash-system-tools src/desktop/system/SystemTools.cpp)
target_include_directories(ludash-system-tools PUBLIC src)
target_link_libraries(ludash-system-tools PUBLIC Qt6::Core)
add_library(ludash-display-settings
    src/desktop/display/DisplaySettings.cpp
    src/desktop/display/BrightnessSettings.cpp
    src/desktop/display/DdcBrightnessSettings.cpp)
target_include_directories(ludash-display-settings PUBLIC src)
target_link_libraries(ludash-display-settings PUBLIC Qt6::Gui ludash-process-runner)
add_library(ludash-input-settings src/desktop/input/InputSettings.cpp)
target_include_directories(ludash-input-settings PUBLIC src)
target_link_libraries(ludash-input-settings PUBLIC Qt6::Core)
add_library(ludash-shell-modules src/shell/modules/ShellModules.hpp src/shell/modules/ShellModuleSchema.cpp src/shell/modules/ShellModules.cpp)
target_include_directories(ludash-shell-modules PUBLIC src)
target_link_libraries(ludash-shell-modules PUBLIC Qt6::Core)
qt_add_resources(ludash-shell-modules module_templates PREFIX /LuDash FILES data/modules/templates/panel/Main.qml data/modules/templates/overview/Main.qml)
add_library(ludash-network src/desktop/network/NetworkStatus.cpp)
target_include_directories(ludash-network PUBLIC src)
target_link_libraries(ludash-network PUBLIC Qt6::Network Qt6::DBus)
add_library(ludash-session-environment src/compositor/session/SessionEnvironment.cpp)
target_include_directories(ludash-session-environment PUBLIC src)
target_link_libraries(ludash-session-environment PUBLIC Qt6::Core Qt6::DBus)
target_compile_definitions(ludash-session-environment PRIVATE LUDASH_ASSET_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/assets")
add_library(ludash-session-actions src/compositor/session/SessionActions.cpp)
target_include_directories(ludash-session-actions PUBLIC src)
target_link_libraries(ludash-session-actions PUBLIC Qt6::Core Qt6::DBus)
add_library(ludash-shortcut-settings src/desktop/shortcuts/ShortcutSettings.cpp)
target_include_directories(ludash-shortcut-settings PUBLIC src)
target_link_libraries(ludash-shortcut-settings
    PUBLIC Qt6::Gui PkgConfig::LUDASH_XKBCOMMON)
add_library(ludash-update-check src/desktop/system/UpdateChecker.hpp src/desktop/system/UpdateChecker.cpp)
target_include_directories(ludash-update-check PUBLIC src)
target_link_libraries(ludash-update-check PUBLIC Qt6::Network)
add_library(ludash-tiling-core src/compositor/tiling/TilingGeometry.c)
target_include_directories(ludash-tiling-core PUBLIC src)
add_library(ludash-system-metrics src/desktop/system/SystemMetrics.c)
target_include_directories(ludash-system-metrics PUBLIC src)
add_library(ludash-tiling
    src/compositor/layout/WindowLayout.cpp
    src/compositor/layout/LayoutTemplates.cpp
    src/compositor/stacking/StackingLayout.cpp
    src/compositor/tiling/TilingLayout.cpp)
target_include_directories(ludash-tiling PUBLIC src)
target_link_libraries(ludash-tiling PUBLIC ludash-tiling-core Qt6::Core)
add_library(ludash-window-rules src/compositor/window/WindowRules.cpp src/compositor/window/WindowSwitcher.cpp)
target_include_directories(ludash-window-rules PUBLIC src)
target_link_libraries(ludash-window-rules PUBLIC Qt6::Core)
add_library(ludash-plugin-catalog
    src/config/plugins/PluginCatalog.cpp
    src/config/plugins/ExtensionRegistry.cpp
    src/config/plugins/ExtensionConfiguration.cpp)
target_include_directories(ludash-plugin-catalog PUBLIC src)
target_link_libraries(ludash-plugin-catalog PUBLIC Qt6::Core)
qt_add_resources(ludash-plugin-catalog extension_targets PREFIX /LunaDash/plugins
    BASE data/plugins FILES data/plugins/targets.json)
add_library(ludash-plugins src/compositor/plugins/PluginManager.hpp
    src/compositor/plugins/PluginManager.cpp src/compositor/plugins/PluginBundle.cpp
    src/compositor/plugins/ExtensionHooks.cpp)
target_include_directories(ludash-plugins PUBLIC src)
target_link_libraries(ludash-plugins PUBLIC ludash-plugin-catalog ludash-tiling Qt6::Core)
add_library(ludash-default-applications src/desktop/app/DefaultApplications.cpp src/desktop/browser/Browser.cpp)
target_include_directories(ludash-default-applications PUBLIC src)
target_link_libraries(ludash-default-applications PUBLIC ludash-configuration Qt6::Core)
add_library(ludash-file-operations src/desktop/fileoperations/FileOperations.cpp)
target_include_directories(ludash-file-operations PUBLIC src)
target_link_libraries(ludash-file-operations PUBLIC Qt6::Core)
add_library(ludash-apps src/desktop/theme/DesktopTheme.cpp src/desktop/app/ApplicationCatalog.cpp src/desktop/app/ApplicationWindow.cpp src/desktop/filemanager/FileManager.cpp src/desktop/filemanager/FileIcons.cpp src/desktop/filemanager/FileIconDelegate.cpp src/desktop/console/Console.cpp src/desktop/system/SystemMonitor.cpp src/desktop/welcome/Welcome.cpp src/desktop/launcher/Launcher.cpp src/desktop/package/PackageManager.cpp src/desktop/plugins/PluginSettings.cpp)
target_include_directories(ludash-apps PUBLIC src)
target_link_libraries(ludash-apps PUBLIC ludash-default-applications ludash-file-operations Qt6::Concurrent ludash-system-metrics ludash-localization ludash-plugin-catalog ludash-wallpaper Qt6::Widgets)
target_compile_options(ludash-apps PRIVATE -Wall -Wextra -Wpedantic)
# wlroots public layer-shell headers include the scanner-generated
# protocol declaration. Some distributions do not install that generated
# header with libwlroots-dev, so generate declarations locally without
# compiling a second protocol implementation.
set(LUDASH_WLR_LAYER_PROTOCOL_HEADER
    ${CMAKE_CURRENT_BINARY_DIR}/wlr-layer-shell-unstable-v1-protocol.h)
add_custom_command(
    OUTPUT ${LUDASH_WLR_LAYER_PROTOCOL_HEADER}
    COMMAND ${WAYLAND_SCANNER} server-header
            ${CMAKE_CURRENT_SOURCE_DIR}/protocols/wlr-layer-shell-unstable-v1.xml
            ${LUDASH_WLR_LAYER_PROTOCOL_HEADER}
    DEPENDS protocols/wlr-layer-shell-unstable-v1.xml
    VERBATIM)

set(LUDASH_XDG_SHELL_PROTOCOL_HEADER
    ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.h)
set(LUDASH_XDG_SHELL_PROTOCOL_XML
    ${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml)
if(NOT EXISTS "${LUDASH_XDG_SHELL_PROTOCOL_XML}")
    message(FATAL_ERROR
        "wayland-protocols xdg-shell XML not found: ${LUDASH_XDG_SHELL_PROTOCOL_XML}")
endif()
add_custom_command(
    OUTPUT ${LUDASH_XDG_SHELL_PROTOCOL_HEADER}
    COMMAND ${WAYLAND_SCANNER} server-header
            ${LUDASH_XDG_SHELL_PROTOCOL_XML}
            ${LUDASH_XDG_SHELL_PROTOCOL_HEADER}
    DEPENDS ${LUDASH_XDG_SHELL_PROTOCOL_XML}
    VERBATIM)

add_library(ludash-thumbnail-readback src/compositor/renderer/capture/ThumbnailReadback.c)
target_include_directories(ludash-thumbnail-readback PUBLIC src)
target_compile_definitions(ludash-thumbnail-readback PRIVATE WLR_USE_UNSTABLE=1)
target_link_libraries(ludash-thumbnail-readback PUBLIC PkgConfig::WLROOTS)
add_library(ludash-wayland
    src/compositor/wayland/WaylandCompositor.cpp
    src/compositor/wayland/Runtime.cpp
    src/compositor/wayland/Output.cpp
    src/compositor/wayland/DisplayConfiguration.cpp
    src/compositor/capture/ScreenCapture.hpp
    src/compositor/capture/ScreenCapture.cpp
    src/compositor/wayland/Surface.cpp
    src/compositor/wayland/XdgPopup.cpp
    src/compositor/input/Input.cpp
    src/compositor/input/TiledPointer.cpp
    src/compositor/input/WindowSwitch.cpp
    src/compositor/session/ClientLaunch.cpp
    src/compositor/input/Keyboard.cpp
    src/compositor/ipc/ControlServer.cpp
    src/desktop/system/SystemStatus.cpp
    ${LUDASH_WLR_LAYER_PROTOCOL_HEADER}
    ${LUDASH_XDG_SHELL_PROTOCOL_HEADER})
target_include_directories(ludash-wayland
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(ludash-wayland
    PRIVATE
        WLR_USE_UNSTABLE=1
        LUDASH_QML_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
        LUDASH_SCRIPT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/scripts")
target_link_libraries(ludash-wayland
    PUBLIC
        ludash-session-environment
        ludash-session-actions
        ludash-shortcut-settings
        ludash-update-check
        ludash-default-applications
        ludash-shell-modules
        ludash-audio-settings
        ludash-power-settings
        ludash-system-tools
        ludash-input-settings
        ludash-display-settings
        ludash-system-metrics
        ludash-xwayland
        ludash-animation
        ludash-configuration
        ludash-network
        ludash-wallpaper
        ludash-tiling
        ludash-window-rules
        ludash-thumbnail-readback
        Qt6::Concurrent
        ludash-localization
        ludash-plugins
        Qt6::Core
        Qt6::Network
        PkgConfig::WAYLAND_SERVER
        PkgConfig::WLROOTS
        PkgConfig::LUDASH_XKBCOMMON)
target_compile_options(ludash-wayland PRIVATE -Wall -Wextra -Wpedantic)

add_library(ludash-client-lifecycle src/desktop/app/WaylandClientShutdown.cpp)
target_include_directories(ludash-client-lifecycle PUBLIC src)
target_link_libraries(ludash-client-lifecycle PUBLIC Qt6::Gui PRIVATE PkgConfig::WAYLAND_CLIENT)
add_executable(ludash-desktop src/desktop/Main.cpp src/desktop/app/DesktopApplication.cpp)
target_link_libraries(ludash-desktop PRIVATE ludash-apps ludash-client-lifecycle)
set_target_properties(ludash-desktop PROPERTIES OUTPUT_NAME lunadash-desktop)
add_custom_command(TARGET ludash-desktop POST_BUILD COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:ludash-desktop> ${CMAKE_CURRENT_BINARY_DIR}/ludash-desktop)
add_executable(ludash-compositor src/compositor/Main.cpp src/compositor/session/SessionApplication.cpp)
target_link_libraries(ludash-compositor PRIVATE ludash-wayland)
set_target_properties(ludash-compositor PROPERTIES OUTPUT_NAME lunadash-compositor)
add_custom_command(TARGET ludash-compositor POST_BUILD COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:ludash-compositor> ${CMAKE_CURRENT_BINARY_DIR}/ludash-compositor)
add_executable(ludashctl src/ctl/Main.cpp src/ctl/command/ControlClient.cpp)
target_include_directories(ludashctl PRIVATE src)
target_link_libraries(ludashctl PRIVATE Qt6::Network)
set_target_properties(ludashctl PROPERTIES OUTPUT_NAME lunadashctl)
add_custom_command(TARGET ludashctl POST_BUILD COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:ludashctl> ${CMAKE_CURRENT_BINARY_DIR}/ludashctl)
include(${CMAKE_CURRENT_LIST_DIR}/modules/Plugins.cmake)
configure_file(data/ludash.desktop.in ludash.desktop @ONLY)
configure_file(data/lunadash.desktop.in lunadash.desktop @ONLY)
configure_file(data/lunadash-app.desktop.in lunadash-app.desktop @ONLY)
install(TARGETS ludash-desktop ludash-compositor ludashctl RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/ludash-desktop ${CMAKE_CURRENT_BINARY_DIR}/ludash-compositor ${CMAKE_CURRENT_BINARY_DIR}/ludashctl DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS scripts/lunadash-session scripts/ludash-session scripts/lunadash-clipboard-bridge DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/lunadash.desktop ${CMAKE_CURRENT_BINARY_DIR}/ludash.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/wayland-sessions)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/lunadash-app.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(DIRECTORY qml/ DESTINATION ${CMAKE_INSTALL_DATADIR}/lunadash/shell PATTERN "digital-clock" EXCLUDE)
install(DIRECTORY data/assets/ DESTINATION ${CMAKE_INSTALL_DATADIR}/lunadash/data/assets)
install(FILES data/assets/lunadash.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps)
install(DIRECTORY data/wallpapers/ DESTINATION ${CMAKE_INSTALL_DATADIR}/ludash/wallpapers)
install(DIRECTORY data/modules/ DESTINATION ${CMAKE_INSTALL_DATADIR}/ludash/modules)
install(DIRECTORY data/translations/ DESTINATION ${CMAKE_INSTALL_DATADIR}/ludash/translations)

# Retained Qt Quick window adapters are compiled even though wlroots owns the
# active scene. This prevents dormant integration sources from silently rotting.
add_library(ludash-window-items
    src/compositor/window/WindowFrame.cpp
    src/compositor/window/ResizeGuide.cpp)
target_include_directories(ludash-window-items PUBLIC src)
target_link_libraries(ludash-window-items PUBLIC Qt6::Quick)
