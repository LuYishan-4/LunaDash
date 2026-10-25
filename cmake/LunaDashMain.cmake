cmake_minimum_required(VERSION 3.21)
project(LunaDash VERSION 1.0.2 LANGUAGES C CXX)
if(NOT DEFINED LUDASH_RELEASE_VERSION)
    set(LUDASH_RELEASE_VERSION "1.0.2a")
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include(GNUInstallDirs)
option(LUDASH_ENABLE_SANITIZERS "Enable ASan and UBSan for project code" OFF)
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
find_package(nlohmann_json 3.11 CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    set(JSON_BuildTests OFF CACHE INTERNAL "Disable dependency tests")
    FetchContent_Declare(nlohmann_json
        URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz
        URL_HASH SHA256=0d8ef5af7f9794e3263480193c491549b2ba6cc74bb018906202ada498a79406
        ${LUDASH_FETCHCONTENT_TIMESTAMP})
    FetchContent_MakeAvailable(nlohmann_json)
endif()
find_package(PkgConfig REQUIRED)
pkg_check_modules(LUDASH_GIO REQUIRED IMPORTED_TARGET gio-2.0)
pkg_check_modules(LUDASH_GDK_PIXBUF REQUIRED IMPORTED_TARGET gdk-pixbuf-2.0)
pkg_check_modules(LUDASH_CURL REQUIRED IMPORTED_TARGET libcurl>=7.85)
pkg_check_modules(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server)
pkg_check_modules(LUDASH_XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)
pkg_search_module(WLROOTS REQUIRED IMPORTED_TARGET
    wlroots-0.20 wlroots-0.19 wlroots-0.18 "wlroots>=0.17")
# wlroots public headers expose pixman and DRM types. Some distributions do
# not propagate these include paths through wlroots' pkg-config metadata.
pkg_check_modules(LUDASH_WLROOTS_HEADERS REQUIRED IMPORTED_TARGET pixman-1 libdrm)
target_link_libraries(PkgConfig::WLROOTS INTERFACE PkgConfig::LUDASH_WLROOTS_HEADERS)
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

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/plugins/macros/Version.hpp.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/plugins/macros/Version.hpp"
    @ONLY
)

add_compile_definitions(
    LUDASH_VERSION="${LUDASH_RELEASE_VERSION}"
    LUDASH_GIT_COMMIT="${LUNADASH_GIT_COMMIT}")

include(${CMAKE_CURRENT_LIST_DIR}/modules/Renderer.cmake)

add_library(ludash-xwayland src/compositor/xwayland/XWaylandSupport.cpp)
target_include_directories(ludash-xwayland PUBLIC src)
target_compile_definitions(ludash-xwayland PRIVATE WLR_USE_UNSTABLE=1)
target_link_libraries(ludash-xwayland PUBLIC
    PkgConfig::WLROOTS PkgConfig::WAYLAND_SERVER)
add_library(ludash-wallpaper src/desktop/wallpaper/WallpaperSettings.cpp
    src/desktop/wallpaper/WallpaperPalette.cpp)
target_include_directories(ludash-wallpaper PUBLIC src)
target_link_libraries(ludash-wallpaper PUBLIC
    ludash-configuration ludash-process-runner
    PkgConfig::LUDASH_GIO PkgConfig::LUDASH_GDK_PIXBUF)
target_compile_definitions(ludash-wallpaper PRIVATE LUDASH_WALLPAPER_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/wallpapers")
add_library(ludash-desktop-preferences
    src/config/desktop/DesktopPreferenceStore.cpp)
target_include_directories(ludash-desktop-preferences PUBLIC src)
target_link_libraries(ludash-desktop-preferences
    PUBLIC nlohmann_json::nlohmann_json)
add_library(ludash-localization src/config/localization/Localization.cpp)
target_include_directories(ludash-localization PUBLIC src)
target_link_libraries(ludash-localization
    PUBLIC ludash-desktop-preferences nlohmann_json::nlohmann_json
    PRIVATE PkgConfig::LUDASH_GIO)
target_compile_definitions(ludash-localization PRIVATE
    LUDASH_TRANSLATIONS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/translations"
    LUDASH_TRANSLATIONS_INSTALL_DIR="${CMAKE_INSTALL_FULL_DATADIR}/ludash/translations")
add_library(ludash-appearance-presets
    src/config/appearance/AppearancePresets.cpp)
target_include_directories(ludash-appearance-presets PUBLIC src)
target_link_libraries(ludash-appearance-presets
    PUBLIC ludash-desktop-preferences nlohmann_json::nlohmann_json)
add_library(ludash-appearance-palette
    src/config/appearance/AppearancePalette.cpp)
target_include_directories(ludash-appearance-palette PUBLIC src)
target_link_libraries(ludash-appearance-palette
    PUBLIC nlohmann_json::nlohmann_json)
add_library(ludash-configuration src/config/desktop/DesktopPreferences.cpp)
target_include_directories(ludash-configuration PUBLIC src)
target_link_libraries(ludash-configuration PUBLIC PkgConfig::LUDASH_GIO
    ludash-desktop-preferences ludash-appearance-presets
    ludash-appearance-palette)
add_library(ludash-json src/core/json/JsonAccess.cpp)
target_include_directories(ludash-json PUBLIC src)
target_link_libraries(ludash-json PUBLIC nlohmann_json::nlohmann_json)
add_library(ludash-text src/core/text/Text.cpp src/core/text/Url.cpp
    src/core/text/Pattern.cpp)
target_include_directories(ludash-text PUBLIC src)
target_link_libraries(ludash-text PRIVATE PkgConfig::LUDASH_GIO)
add_library(ludash-file-io src/core/filesystem/FileIo.cpp
    src/core/filesystem/TemporaryDirectory.cpp)
target_include_directories(ludash-file-io PUBLIC src)
add_library(ludash-log src/core/log/Log.cpp)
target_include_directories(ludash-log PUBLIC src)
add_library(ludash-process-runner src/core/process/ProcessRunner.cpp)
target_include_directories(ludash-process-runner PUBLIC src)
target_link_libraries(ludash-process-runner PUBLIC PkgConfig::WAYLAND_SERVER)
add_library(ludash-crypto src/core/crypto/Sha256.cpp)
target_include_directories(ludash-crypto PUBLIC src)
target_link_libraries(ludash-crypto PUBLIC PkgConfig::LUDASH_GIO)
add_library(ludash-audio-settings src/desktop/audio/AudioSettings.cpp)
target_include_directories(ludash-audio-settings PUBLIC src)
target_link_libraries(ludash-audio-settings PUBLIC
    ludash-process-runner nlohmann_json::nlohmann_json)
add_library(ludash-power-settings src/desktop/power/PowerSettings.cpp)
target_include_directories(ludash-power-settings PUBLIC src)
target_link_libraries(ludash-power-settings PUBLIC
    ludash-process-runner nlohmann_json::nlohmann_json)
add_library(ludash-system-tools src/desktop/system/SystemTools.cpp)
target_include_directories(ludash-system-tools PUBLIC src)
add_library(ludash-display-settings
    src/desktop/display/BrightnessSettings.cpp
    src/desktop/display/DdcBrightnessSettings.cpp)
target_include_directories(ludash-display-settings PUBLIC src)
target_link_libraries(ludash-display-settings PUBLIC
    ludash-process-runner nlohmann_json::nlohmann_json)
add_library(ludash-input-settings src/desktop/input/InputSettings.cpp)
target_include_directories(ludash-input-settings PUBLIC src)
target_link_libraries(ludash-input-settings PUBLIC PkgConfig::LUDASH_XKBCOMMON)
add_library(ludash-shell-modules
    src/shell/modules/ShellModuleSchema.cpp
    src/shell/modules/ShellModules.cpp)
target_include_directories(ludash-shell-modules PUBLIC src)
target_link_libraries(ludash-shell-modules
    PUBLIC nlohmann_json::nlohmann_json
    PRIVATE PkgConfig::LUDASH_GIO)
target_compile_definitions(ludash-shell-modules PRIVATE
    LUDASH_MODULE_REGISTRY_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/data/modules/registry.json"
    LUDASH_MODULE_REGISTRY_INSTALL="${CMAKE_INSTALL_FULL_DATADIR}/ludash/modules/registry.json")
add_library(ludash-orbit-settings src/shell/launcher/OrbitSettings.cpp)
target_include_directories(ludash-orbit-settings PUBLIC src)
target_link_libraries(ludash-orbit-settings
    PUBLIC nlohmann_json::nlohmann_json
    PRIVATE PkgConfig::LUDASH_GIO)
target_compile_definitions(ludash-orbit-settings PRIVATE
    LUDASH_ORBIT_DEFAULTS_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/data/launcher/orbit.json"
    LUDASH_ORBIT_DEFAULTS_INSTALL="${CMAKE_INSTALL_FULL_DATADIR}/ludash/launcher/orbit.json")
install(FILES data/launcher/orbit.json DESTINATION ${CMAKE_INSTALL_DATADIR}/ludash/launcher)
add_library(ludash-theme-sync src/desktop/theme/ThemeSync.cpp
    src/desktop/theme/FcitxTheme.cpp)
target_include_directories(ludash-theme-sync PUBLIC src)
target_link_libraries(ludash-theme-sync
    PUBLIC nlohmann_json::nlohmann_json
    PRIVATE PkgConfig::LUDASH_GIO)
target_compile_definitions(ludash-theme-sync PRIVATE
    LUDASH_THEME_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/themes"
    LUDASH_THEME_INSTALL_DIR="${CMAKE_INSTALL_FULL_DATADIR}/lunadash/themes")
add_library(ludash-weather src/desktop/weather/WeatherStatus.cpp)
target_include_directories(ludash-weather PUBLIC src)
target_link_libraries(ludash-weather PUBLIC
    nlohmann_json::nlohmann_json PkgConfig::LUDASH_CURL
    PkgConfig::WAYLAND_SERVER)
add_library(ludash-network src/desktop/network/NetworkStatus.cpp)
target_include_directories(ludash-network PUBLIC src)
target_link_libraries(ludash-network PUBLIC
    ludash-process-runner PkgConfig::LUDASH_GIO nlohmann_json::nlohmann_json)
add_library(ludash-session-environment src/compositor/session/SessionEnvironment.cpp)
target_include_directories(ludash-session-environment PUBLIC src)
target_compile_definitions(ludash-session-environment PRIVATE
    LUDASH_ASSET_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/assets")
target_link_libraries(ludash-session-environment
    PUBLIC nlohmann_json::nlohmann_json)
add_library(ludash-session-services
    src/compositor/session/SessionServices.cpp)
target_include_directories(ludash-session-services PUBLIC src)
target_link_libraries(ludash-session-services PUBLIC ludash-process-runner)
target_compile_definitions(ludash-session-services PRIVATE
    LUDASH_XDPW_CONFIG_PATH="${CMAKE_INSTALL_FULL_SYSCONFDIR}/xdg/xdg-desktop-portal-wlr/LunaDash")
add_library(ludash-launch-policy src/compositor/session/LaunchPolicy.cpp)
target_include_directories(ludash-launch-policy PUBLIC src)
target_link_libraries(ludash-launch-policy PRIVATE nlohmann_json::nlohmann_json)
target_compile_definitions(ludash-launch-policy PRIVATE
    LUDASH_LAUNCH_CAPABILITIES_INSTALL_FILE="${CMAKE_INSTALL_FULL_DATADIR}/lunadash/session/launch-capabilities.json"
    LUDASH_LAUNCH_CAPABILITIES_SOURCE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/data/session/launch-capabilities.json")
add_library(ludash-screen-capture src/compositor/capture/ScreenCapture.cpp)
target_include_directories(ludash-screen-capture PUBLIC src)
target_link_libraries(ludash-screen-capture PUBLIC ludash-process-runner)
add_library(ludash-session-config src/config/session/SessionConfig.cpp)
target_include_directories(ludash-session-config PUBLIC src)
target_link_libraries(ludash-session-config PUBLIC nlohmann_json::nlohmann_json)
add_library(ludash-desktop-notification src/compositor/session/DesktopNotification.cpp)
target_include_directories(ludash-desktop-notification PUBLIC src)
target_link_libraries(ludash-desktop-notification
    PRIVATE ludash-localization PkgConfig::LUDASH_GIO)
add_library(ludash-session-actions src/compositor/session/SessionActions.cpp)
target_include_directories(ludash-session-actions PUBLIC src)
target_link_libraries(ludash-session-actions PRIVATE PkgConfig::LUDASH_GIO)
add_library(ludash-shortcut-settings src/desktop/shortcuts/ShortcutSettings.cpp)
target_include_directories(ludash-shortcut-settings PUBLIC src)
target_link_libraries(ludash-shortcut-settings
    PUBLIC nlohmann_json::nlohmann_json PkgConfig::LUDASH_XKBCOMMON
    PRIVATE ludash-desktop-preferences)
add_library(ludash-update-check src/desktop/system/UpdateChecker.hpp src/desktop/system/UpdateChecker.cpp)
target_include_directories(ludash-update-check PUBLIC src)
target_link_libraries(ludash-update-check PUBLIC
    nlohmann_json::nlohmann_json PkgConfig::LUDASH_CURL
    PkgConfig::WAYLAND_SERVER)
add_library(ludash-tiling-core src/compositor/tiling/TilingGeometry.c)
target_include_directories(ludash-tiling-core PUBLIC src)
add_library(ludash-system-metrics src/desktop/system/SystemMetrics.c)
target_include_directories(ludash-system-metrics PUBLIC src)
add_library(ludash-system-status src/desktop/system/SystemStatus.cpp)
target_include_directories(ludash-system-status PUBLIC src)
target_link_libraries(ludash-system-status
    PUBLIC ludash-system-metrics ludash-process-runner nlohmann_json::nlohmann_json
    PkgConfig::WAYLAND_SERVER)
add_library(ludash-tiling
    src/compositor/layout/WindowLayout.cpp
    src/compositor/window/WindowTemplate.cpp
    src/compositor/layout/FreeformLayout.cpp
    src/compositor/tiling/TilingLayout.cpp)
target_include_directories(ludash-tiling PUBLIC src)
target_link_libraries(ludash-tiling PUBLIC ludash-tiling-core ludash-json ludash-text)
add_library(ludash-window-rules src/compositor/window/WindowRules.cpp src/compositor/window/WindowSwitcher.cpp)
target_include_directories(ludash-window-rules PUBLIC src)
target_link_libraries(ludash-window-rules PUBLIC
    ludash-tiling ludash-process-runner ludash-file-io ludash-text)
add_library(ludash-plugin-catalog
    src/config/plugins/PluginCatalog.cpp
    src/config/plugins/ExtensionRegistry.cpp
    src/config/plugins/ExtensionConfiguration.cpp)
target_include_directories(ludash-plugin-catalog PUBLIC src)
target_link_libraries(ludash-plugin-catalog PUBLIC
    ludash-configuration ludash-crypto ludash-json ludash-file-io ludash-text)
target_compile_definitions(ludash-plugin-catalog PRIVATE
    LUDASH_PLUGIN_TARGETS_SOURCE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/data/plugins/targets.json"
    LUDASH_PLUGIN_TARGETS_INSTALL_FILE="${CMAKE_INSTALL_FULL_DATADIR}/lunadash/plugins/targets.json")
install(FILES data/plugins/targets.json data/plugins/catalog.json
    DESTINATION "${CMAKE_INSTALL_DATADIR}/lunadash/plugins")
add_library(ludash-plugins src/compositor/plugins/PluginManager.hpp
    src/compositor/plugins/PluginManager.cpp
    src/compositor/plugins/ExtensionHooks.cpp)
target_include_directories(ludash-plugins PUBLIC src)
target_link_libraries(ludash-plugins PUBLIC
    ludash-plugin-catalog ludash-tiling ludash-crypto ludash-process-runner
    ludash-file-io ludash-text ${CMAKE_DL_LIBS})
target_compile_definitions(ludash-plugins PRIVATE
    LUDASH_PLUGIN_CATALOG_SOURCE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/data/plugins/catalog.json"
    LUDASH_PLUGIN_CATALOG_INSTALL_FILE="${CMAKE_INSTALL_FULL_DATADIR}/lunadash/plugins/catalog.json")
add_library(ludash-default-applications src/desktop/app/DefaultApplications.cpp src/desktop/browser/Browser.cpp)
target_include_directories(ludash-default-applications PUBLIC src)
target_link_libraries(ludash-default-applications PUBLIC nlohmann_json::nlohmann_json)
add_library(ludash-package-policy src/desktop/package/PackageManager.cpp)
target_include_directories(ludash-package-policy PUBLIC src)
add_executable(ludash-welcome
    src/desktop/welcome/Main.cpp
    src/desktop/welcome/WelcomeApplication.hpp
    src/desktop/welcome/WelcomeApplication.cpp)
target_include_directories(ludash-welcome PRIVATE src)
target_link_libraries(ludash-welcome PRIVATE Slint::Slint nlohmann_json::nlohmann_json)
slint_target_sources(ludash-welcome src/desktop/welcome/Welcome.slint)
set_target_properties(ludash-welcome PROPERTIES OUTPUT_NAME lunadash-welcome)
add_custom_command(TARGET ludash-welcome POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        $<TARGET_FILE_NAME:ludash-welcome>
        ${CMAKE_CURRENT_BINARY_DIR}/ludash-welcome)
add_executable(ludash-packages
    src/desktop/package/Main.cpp
    src/desktop/package/PackageManagerApplication.hpp
    src/desktop/package/PackageManagerApplication.cpp)
target_include_directories(ludash-packages PRIVATE src)
target_link_libraries(ludash-packages PRIVATE
    Slint::Slint ludash-package-policy ludash-default-applications
    ludash-desktop-preferences ludash-appearance-palette
    ludash-localization PkgConfig::LUDASH_GIO nlohmann_json::nlohmann_json)
slint_target_sources(ludash-packages
    src/desktop/package/PackageManager.slint)
set_target_properties(ludash-packages PROPERTIES
    OUTPUT_NAME lunadash-packages)
add_custom_command(TARGET ludash-packages POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        $<TARGET_FILE_NAME:ludash-packages>
        ${CMAKE_CURRENT_BINARY_DIR}/ludash-packages)
add_executable(ludash-screencast-chooser
    src/service/portal/screencast/Main.cpp
    src/service/portal/screencast/ScreenCastChooserApplication.hpp
    src/service/portal/screencast/ScreenCastChooserApplication.cpp)
target_include_directories(ludash-screencast-chooser PRIVATE src)
target_compile_definitions(ludash-screencast-chooser PRIVATE
    LUDASH_TRANSLATIONS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/translations"
    LUDASH_TRANSLATIONS_INSTALL_DIR="${CMAKE_INSTALL_FULL_DATADIR}/ludash/translations")
target_link_libraries(ludash-screencast-chooser PRIVATE
    Slint::Slint ludash-process-runner nlohmann_json::nlohmann_json)
slint_target_sources(ludash-screencast-chooser
    src/service/portal/screencast/ScreenCastChooser.slint)
set_target_properties(ludash-screencast-chooser PROPERTIES
    OUTPUT_NAME lunadash-screencast-chooser)
install(TARGETS ludash-screencast-chooser RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
add_executable(ludash-filepicker
    src/service/portal/filepicker/Main.cpp
    src/service/portal/filepicker/FilePickerApplication.hpp
    src/service/portal/filepicker/FilePickerApplication.cpp)
target_include_directories(ludash-filepicker PRIVATE src)
target_compile_definitions(ludash-filepicker PRIVATE
    LUDASH_TRANSLATIONS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/translations"
    LUDASH_TRANSLATIONS_INSTALL_DIR="${CMAKE_INSTALL_FULL_DATADIR}/ludash/translations")
target_link_libraries(ludash-filepicker PRIVATE
    Slint::Slint ludash-process-runner PkgConfig::LUDASH_GDK_PIXBUF
    PkgConfig::LUDASH_GIO
    nlohmann_json::nlohmann_json)
slint_target_sources(ludash-filepicker
    src/service/portal/filepicker/FilePicker.slint)
set_target_properties(ludash-filepicker PROPERTIES OUTPUT_NAME lunadash-filepicker)
install(TARGETS ludash-filepicker RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
# LunaDash's Slint windows compile their icons (data/icons) into the executable, so an
# installation never refers back to the source tree.
set_target_properties(ludash-welcome ludash-packages ludash-screencast-chooser
    ludash-filepicker PROPERTIES SLINT_EMBED_RESOURCES embed-files)
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

# wlroots 0.17-0.19 include the generated cursor-shape declarations from wlr_cursor_shape_v1.h.
set(LUDASH_CURSOR_SHAPE_PROTOCOL_HEADER
    ${CMAKE_CURRENT_BINARY_DIR}/cursor-shape-v1-protocol.h)
set(LUDASH_CURSOR_SHAPE_PROTOCOL_XML
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/cursor-shape/cursor-shape-v1.xml)
if(NOT EXISTS "${LUDASH_CURSOR_SHAPE_PROTOCOL_XML}")
    message(FATAL_ERROR
        "wayland-protocols cursor-shape XML not found (wayland-protocols 1.32 or newer is required): ${LUDASH_CURSOR_SHAPE_PROTOCOL_XML}")
endif()
add_custom_command(
    OUTPUT ${LUDASH_CURSOR_SHAPE_PROTOCOL_HEADER}
    COMMAND ${WAYLAND_SCANNER} server-header
            ${LUDASH_CURSOR_SHAPE_PROTOCOL_XML}
            ${LUDASH_CURSOR_SHAPE_PROTOCOL_HEADER}
    DEPENDS ${LUDASH_CURSOR_SHAPE_PROTOCOL_XML}
    VERBATIM)

add_custom_target(ludash-wlroots-protocol-headers
    DEPENDS ${LUDASH_WLR_LAYER_PROTOCOL_HEADER} ${LUDASH_XDG_SHELL_PROTOCOL_HEADER}
            ${LUDASH_CURSOR_SHAPE_PROTOCOL_HEADER})
target_include_directories(ludash-xwayland PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
add_dependencies(ludash-xwayland ludash-wlroots-protocol-headers)

# wlroots 0.19 exposes capture protocol enums in its public headers but does
# not install the generated declarations on every distribution.
if(WLROOTS_VERSION VERSION_GREATER_EQUAL "0.19")
    set(LUDASH_EXT_CAPTURE_PROTOCOL_HEADER
        ${CMAKE_CURRENT_BINARY_DIR}/ext-image-copy-capture-v1-protocol.h)
    set(LUDASH_EXT_CAPTURE_PROTOCOL_XML
        ${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-image-copy-capture/ext-image-copy-capture-v1.xml)
    if(NOT EXISTS "${LUDASH_EXT_CAPTURE_PROTOCOL_XML}")
        message(FATAL_ERROR
            "wayland-protocols capture XML not found: ${LUDASH_EXT_CAPTURE_PROTOCOL_XML}")
    endif()
    add_custom_command(
        OUTPUT ${LUDASH_EXT_CAPTURE_PROTOCOL_HEADER}
        COMMAND ${WAYLAND_SCANNER} server-header
                ${LUDASH_EXT_CAPTURE_PROTOCOL_XML}
                ${LUDASH_EXT_CAPTURE_PROTOCOL_HEADER}
        DEPENDS ${LUDASH_EXT_CAPTURE_PROTOCOL_XML}
        VERBATIM)
endif()

add_library(ludash-thumbnail-readback src/compositor/renderer/capture/ThumbnailReadback.c)
target_include_directories(ludash-thumbnail-readback PUBLIC src)
target_compile_definitions(ludash-thumbnail-readback PRIVATE WLR_USE_UNSTABLE=1)
target_link_libraries(ludash-thumbnail-readback PUBLIC PkgConfig::WLROOTS)
add_library(ludash-night-color src/compositor/renderer/color/NightColor.c)
target_include_directories(ludash-night-color PUBLIC src)
target_compile_definitions(ludash-night-color PRIVATE WLR_USE_UNSTABLE=1)
target_link_libraries(ludash-night-color PUBLIC PkgConfig::WLROOTS m)
add_library(ludash-scene-backdrop
    src/compositor/renderer/blur/SceneBackdrop.h
    src/compositor/renderer/blur/SceneBackdrop.c)
target_include_directories(ludash-scene-backdrop PUBLIC src)
target_compile_definitions(ludash-scene-backdrop PRIVATE WLR_USE_UNSTABLE=1)
target_link_libraries(ludash-scene-backdrop PUBLIC PkgConfig::WLROOTS m)
add_library(ludash-corner-geometry
    src/compositor/renderer/clip/CornerGeometry.h
    src/compositor/renderer/clip/CornerGeometry.c)
target_include_directories(ludash-corner-geometry PUBLIC src)
target_link_libraries(ludash-corner-geometry PUBLIC m)
add_library(ludash-scene-window-effects
    src/compositor/renderer/clip/WindowCorners.hpp
    src/compositor/renderer/clip/WindowCorners.cpp
    src/compositor/renderer/blur/WindowGlass.hpp
    src/compositor/renderer/blur/WindowGlass.cpp)
target_include_directories(ludash-scene-window-effects PUBLIC src)
target_include_directories(ludash-scene-window-effects PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
add_dependencies(ludash-scene-window-effects ludash-wlroots-protocol-headers)
target_compile_definitions(ludash-scene-window-effects PRIVATE WLR_USE_UNSTABLE=1)
target_link_libraries(ludash-scene-window-effects
    PUBLIC ludash-scene-backdrop ludash-corner-geometry
           PkgConfig::WAYLAND_SERVER PkgConfig::WLROOTS)
add_library(ludash-wayland
    src/compositor/wayland/NightLight.cpp
    src/compositor/window/Scratchpad.cpp
    src/compositor/wayland/WaylandCompositor.cpp
    src/compositor/wayland/DebugSnapshot.cpp
    src/compositor/wayland/Register.cpp
    src/compositor/wayland/Output.cpp
    src/compositor/wayland/DisplayConfiguration.cpp
    src/compositor/wayland/Surface.cpp
    src/compositor/wayland/XWayland.cpp
    src/compositor/wayland/XdgPopup.cpp
    src/compositor/input/Input.cpp
    src/compositor/settings/SettingsApi.cpp
    src/compositor/window/WindowPointer.cpp
    src/compositor/window/WindowSwitch.cpp
    src/compositor/input/Keyboard.cpp
    src/compositor/ipc/ControlServer.cpp
    ${LUDASH_WLR_LAYER_PROTOCOL_HEADER}
    ${LUDASH_XDG_SHELL_PROTOCOL_HEADER}
    ${LUDASH_CURSOR_SHAPE_PROTOCOL_HEADER}
    ${LUDASH_EXT_CAPTURE_PROTOCOL_HEADER})
target_include_directories(ludash-wayland
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(ludash-wayland
    PRIVATE
        WLR_USE_UNSTABLE=1
        LUDASH_SHELL_SOURCE_DIR="${LUDASH_SHELL_SOURCE_DIR}"
        LUDASH_SCRIPT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/scripts")
target_link_libraries(ludash-wayland
    PUBLIC
        ludash-screen-capture
        ludash-session-config
        ludash-session-environment
        ludash-session-services
        ludash-render-selection
        ludash-shell-renderer
        ludash-launch-policy
        ludash-session-actions
        ludash-desktop-notification
        ludash-shortcut-settings
        ludash-update-check
        ludash-default-applications
        ludash-shell-modules
        ludash-orbit-settings
        ludash-audio-settings
        ludash-power-settings
        ludash-system-tools
        ludash-input-settings
        ludash-display-settings
        ludash-system-status
        ludash-xwayland
        ludash-animation
        ludash-configuration
        ludash-network
        ludash-wallpaper
        ludash-tiling
        ludash-window-rules
        ludash-thumbnail-readback
        ludash-scene-window-effects
        ludash-night-color
        ludash-theme-sync
        ludash-weather
        ludash-localization
        ludash-plugins
        ludash-crypto
        ludash-json
        ludash-text
        ludash-file-io
        ludash-log
        PkgConfig::WAYLAND_SERVER
        PkgConfig::WLROOTS
        PkgConfig::LUDASH_XKBCOMMON)
target_compile_options(ludash-wayland PRIVATE -Wall -Wextra -Wpedantic)

add_executable(ludash-desktop src/desktop/Main.cpp src/desktop/app/DesktopApplication.cpp)
target_include_directories(ludash-desktop PRIVATE src)
target_link_libraries(ludash-desktop PRIVATE ludash-default-applications)
add_dependencies(ludash-desktop ludash-welcome ludash-packages)
set_target_properties(ludash-desktop PROPERTIES
    OUTPUT_NAME lunadash-desktop)
add_custom_command(TARGET ludash-desktop POST_BUILD COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:ludash-desktop> ${CMAKE_CURRENT_BINARY_DIR}/ludash-desktop)
add_executable(ludash-compositor src/compositor/Main.cpp src/compositor/session/SessionApplication.cpp)
target_link_libraries(ludash-compositor PRIVATE ludash-wayland)
set_target_properties(ludash-compositor PROPERTIES OUTPUT_NAME lunadash-compositor)
add_custom_command(TARGET ludash-compositor POST_BUILD COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:ludash-compositor> ${CMAKE_CURRENT_BINARY_DIR}/ludash-compositor)
add_executable(ludashctl src/ctl/Main.cpp src/ctl/command/ControlClient.cpp
    src/ctl/command/DebugCommand.cpp)
target_include_directories(ludashctl PRIVATE src)
target_link_libraries(ludashctl PRIVATE nlohmann_json::nlohmann_json)
set_target_properties(ludashctl PROPERTIES OUTPUT_NAME lunadashctl)
add_custom_command(TARGET ludashctl POST_BUILD COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:ludashctl> ${CMAKE_CURRENT_BINARY_DIR}/ludashctl)
include(${CMAKE_CURRENT_LIST_DIR}/modules/Plugins.cmake)
configure_file(data/ludash.desktop.in ludash.desktop @ONLY)
configure_file(data/lunadash.desktop.in lunadash.desktop @ONLY)
configure_file(data/lunadash-app.desktop.in lunadash-app.desktop @ONLY)
install(TARGETS ludash-desktop ludash-welcome ludash-packages ludash-compositor ludashctl RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/ludash-desktop ${CMAKE_CURRENT_BINARY_DIR}/ludash-welcome ${CMAKE_CURRENT_BINARY_DIR}/ludash-packages ${CMAKE_CURRENT_BINARY_DIR}/ludash-compositor ${CMAKE_CURRENT_BINARY_DIR}/ludashctl DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS scripts/lunadash-session scripts/ludash-session scripts/lunadash-clipboard-history DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/lunadash.desktop ${CMAKE_CURRENT_BINARY_DIR}/ludash.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/wayland-sessions)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/lunadash-app.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
# Slint shell files (and, for a legacy Quickshell checkout, QML files).
if(LUDASH_INSTALL_SHELL)
    install(DIRECTORY "${LUDASH_SHELL_SOURCE_DIR}/"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/lunadash/shell
        FILES_MATCHING
            REGEX ".*\\.(slint|po|qml|js|svg|png|jpg|jpeg|webp|gif)$"
            PATTERN "theme.json"
            PATTERN "LICENSE"
            PATTERN "qmldir"
            PATTERN ".git" EXCLUDE
            PATTERN ".github" EXCLUDE
            PATTERN "scripts" EXCLUDE
            PATTERN "tests" EXCLUDE)
else()
    # Without the LunaDash shell the system default runs none; a user's session.json can name
    # a custom shell (docs/en/SHELL.md).
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/session.json" "{\"shell\": \"none\"}\n")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/session.json"
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/xdg/LunaDash)
endif()
install(DIRECTORY data/assets/ DESTINATION ${CMAKE_INSTALL_DATADIR}/lunadash/data/assets)
install(FILES data/assets/lunadash.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps)
install(DIRECTORY data/wallpapers/ DESTINATION ${CMAKE_INSTALL_DATADIR}/ludash/wallpapers)
install(FILES data/session/launch-capabilities.json
    DESTINATION ${CMAKE_INSTALL_DATADIR}/lunadash/session)
install(DIRECTORY data/modules/ DESTINATION ${CMAKE_INSTALL_DATADIR}/ludash/modules)
install(DIRECTORY data/translations/ DESTINATION ${CMAKE_INSTALL_DATADIR}/ludash/translations)

install(DIRECTORY data/themes/ DESTINATION ${CMAKE_INSTALL_DATADIR}/lunadash/themes)
