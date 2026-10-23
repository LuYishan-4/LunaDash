option(LUDASH_BUILD_RENDERER_TESTS "Build embedded shader and GL lifetime regression tests" OFF)
if(LUDASH_BUILD_RENDERER_TESTS)
    enable_testing()
    add_executable(lunadash-renderer-test tests/renderer/RendererTests.cpp)
    target_link_libraries(lunadash-renderer-test PRIVATE ludash-renderer)
    add_test(NAME lunadash-renderer COMMAND lunadash-renderer-test --opengl)
    set_tests_properties(lunadash-renderer PROPERTIES
        ENVIRONMENT "EGL_PLATFORM=surfaceless;LIBGL_ALWAYS_SOFTWARE=1" TIMEOUT 30)
    # Explicit test component for staged-install relocation verification only.
    install(TARGETS lunadash-renderer-test
        RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/lunadash/tests
        COMPONENT Tests EXCLUDE_FROM_ALL)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test Widgets)
    add_executable(lunadash-input-settings-test
        tests/input/InputSettingsTests.cpp)
    set_target_properties(lunadash-input-settings-test PROPERTIES AUTOMOC OFF)
    target_link_libraries(lunadash-input-settings-test
        PRIVATE ludash-input-settings)
    add_test(NAME lunadash-input-settings COMMAND lunadash-input-settings-test)

    add_executable(lunadash-process-runner-test
        tests/process/ProcessRunnerTests.cpp)
    set_target_properties(lunadash-process-runner-test PROPERTIES AUTOMOC OFF)
    target_link_libraries(lunadash-process-runner-test
        PRIVATE ludash-process-runner)
    add_test(NAME lunadash-process-runner COMMAND lunadash-process-runner-test)
    set_tests_properties(lunadash-process-runner PROPERTIES TIMEOUT 15)

    add_executable(lunadash-system-status-test tests/system/SystemStatusTests.cpp)
    set_target_properties(lunadash-system-status-test PROPERTIES AUTOMOC OFF)
    target_link_libraries(lunadash-system-status-test PRIVATE ludash-system-status)
    add_test(NAME lunadash-system-status COMMAND lunadash-system-status-test)
    set_tests_properties(lunadash-system-status PROPERTIES TIMEOUT 10)

    add_executable(lunadash-network-status-test tests/network/NetworkStatusTests.cpp)
    set_target_properties(lunadash-network-status-test PROPERTIES AUTOMOC OFF)
    target_link_libraries(lunadash-network-status-test PRIVATE ludash-network)
    add_test(NAME lunadash-network-status COMMAND lunadash-network-status-test)
    set_tests_properties(lunadash-network-status PROPERTIES TIMEOUT 10)

    add_executable(lunadash-weather-status-test tests/weather/WeatherStatusTests.cpp)
    set_target_properties(lunadash-weather-status-test PROPERTIES AUTOMOC OFF)
    target_link_libraries(lunadash-weather-status-test PRIVATE ludash-weather)
    add_test(NAME lunadash-weather-status COMMAND lunadash-weather-status-test)
    set_tests_properties(lunadash-weather-status PROPERTIES TIMEOUT 10)

    add_executable(lunadash-audio-settings-test
        tests/audio/AudioSettingsTests.cpp)
    set_target_properties(lunadash-audio-settings-test PROPERTIES AUTOMOC OFF)
    target_link_libraries(lunadash-audio-settings-test
        PRIVATE ludash-audio-settings)
    add_test(NAME lunadash-audio-settings COMMAND lunadash-audio-settings-test)

    add_executable(lunadash-power-settings-test
        tests/power/PowerSettingsTests.cpp)
    set_target_properties(lunadash-power-settings-test PROPERTIES AUTOMOC OFF)
    target_link_libraries(lunadash-power-settings-test
        PRIVATE ludash-power-settings)
    add_test(NAME lunadash-power-settings COMMAND lunadash-power-settings-test)

    add_executable(lunadash-display-settings-test
        tests/display/DisplaySettingsTests.cpp)
    set_target_properties(lunadash-display-settings-test PROPERTIES AUTOMOC OFF)
    target_link_libraries(lunadash-display-settings-test
        PRIVATE ludash-display-settings)
    add_test(NAME lunadash-display-settings COMMAND lunadash-display-settings-test)

    add_executable(lunadash-update-check-test
        tests/system/UpdateCheckerTests.cpp)
    set_target_properties(lunadash-update-check-test PROPERTIES AUTOMOC OFF)
    target_link_libraries(lunadash-update-check-test PRIVATE ludash-update-check)
    add_test(NAME lunadash-update-check COMMAND lunadash-update-check-test)

    add_executable(lunadash-desktop-preferences-test
        tests/config/DesktopPreferenceStoreTests.cpp)
    set_target_properties(lunadash-desktop-preferences-test
        PROPERTIES AUTOMOC OFF)
    target_link_libraries(lunadash-desktop-preferences-test
        PRIVATE ludash-desktop-preferences)
    add_test(NAME lunadash-desktop-preferences
        COMMAND lunadash-desktop-preferences-test)

    add_executable(lunadash-localization-test tests/localization/main.cpp)
    set_target_properties(lunadash-localization-test PROPERTIES AUTOMOC OFF)
    target_link_libraries(lunadash-localization-test PRIVATE ludash-localization)
    add_test(NAME lunadash-localization
        COMMAND lunadash-localization-test
            ${CMAKE_CURRENT_SOURCE_DIR}/data/translations)

    add_executable(lunadash-desktop-preferences-adapter-test
        tests/config/DesktopPreferencesAdapterTests.cpp)
    set_target_properties(lunadash-desktop-preferences-adapter-test
        PROPERTIES AUTOMOC OFF)
    target_link_libraries(lunadash-desktop-preferences-adapter-test
        PRIVATE ludash-configuration ludash-desktop-preferences Qt6::Core)
    add_test(NAME lunadash-desktop-preferences-adapter
        COMMAND lunadash-desktop-preferences-adapter-test)

    add_executable(lunadash-appearance-palette-test
        tests/settings/AppearancePaletteTests.cpp)
    set_target_properties(lunadash-appearance-palette-test
        PROPERTIES AUTOMOC OFF)
    target_link_libraries(lunadash-appearance-palette-test
        PRIVATE ludash-appearance-palette)
    add_test(NAME lunadash-appearance-palette
        COMMAND lunadash-appearance-palette-test)

    add_executable(lunadash-shortcut-settings-test
        tests/shortcuts/ShortcutSettingsTests.cpp)
    set_target_properties(lunadash-shortcut-settings-test
        PROPERTIES AUTOMOC OFF)
    target_link_libraries(lunadash-shortcut-settings-test
        PRIVATE ludash-shortcut-settings ludash-desktop-preferences)
    add_test(NAME lunadash-shortcut-settings
        COMMAND lunadash-shortcut-settings-test)

    add_executable(lunadash-session-environment-test
        tests/session/SessionEnvironmentTests.cpp)
    set_target_properties(lunadash-session-environment-test
        PROPERTIES AUTOMOC OFF)
    target_link_libraries(lunadash-session-environment-test
        PRIVATE ludash-session-environment nlohmann_json::nlohmann_json)
    add_test(NAME lunadash-session-environment
        COMMAND lunadash-session-environment-test)

    add_executable(lunadash-session-services-test
        tests/session/SessionServicesTests.cpp)
    set_target_properties(lunadash-session-services-test
        PROPERTIES AUTOMOC OFF)
    target_link_libraries(lunadash-session-services-test
        PRIVATE ludash-session-services)
    add_test(NAME lunadash-session-services
        COMMAND lunadash-session-services-test)
    set_tests_properties(lunadash-session-services PROPERTIES TIMEOUT 15)

    add_executable(lunadash-qt-toolkit-client tests/wayland/QtToolkitClient.cpp)
    target_link_libraries(lunadash-qt-toolkit-client PRIVATE Qt6::Widgets)
    add_executable(lunadash-package-policy-test
        tests/package/PackageManagerTests.cpp)
    set_target_properties(lunadash-package-policy-test PROPERTIES AUTOMOC OFF)
    target_link_libraries(lunadash-package-policy-test PRIVATE ludash-package-policy)
    add_test(NAME lunadash-package-policy COMMAND lunadash-package-policy-test)
    add_test(NAME lunadash-installer COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/installer/test_install.py)
    set_tests_properties(lunadash-installer PROPERTIES TIMEOUT 90)
    add_executable(lunadash-shell-rendering-test tests/renderer/ShellRenderingTests.cpp)
    target_link_libraries(lunadash-shell-rendering-test PRIVATE ludash-shell-renderer)
    add_test(NAME lunadash-shell-rendering COMMAND lunadash-shell-rendering-test)
    add_executable(lunadash-portal-picker-test tests/files/PortalPickerTests.cpp
        src/service/portal/FileChooserOptions.cpp
        src/service/portal/FileChooserOptions.hpp)
    target_include_directories(lunadash-portal-picker-test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(lunadash-portal-picker-test PRIVATE Qt6::Test)
    add_test(NAME lunadash-portal-picker COMMAND lunadash-portal-picker-test)
    set_tests_properties(lunadash-portal-picker PROPERTIES TIMEOUT 30)
    add_executable(lunadash-wallpaper-library-test tests/wallpaper/WallpaperLibraryTests.cpp)
    target_link_libraries(lunadash-wallpaper-library-test PRIVATE ludash-wallpaper Qt6::Test)
    add_test(NAME lunadash-wallpaper-library COMMAND lunadash-wallpaper-library-test)
    set_tests_properties(lunadash-wallpaper-library PROPERTIES TIMEOUT 30)
    add_executable(lunadash-audio-spectrum-test tests/media/AudioSpectrumTests.cpp)
    set_target_properties(lunadash-audio-spectrum-test PROPERTIES AUTOMOC OFF)
    target_link_libraries(lunadash-audio-spectrum-test PRIVATE ludash-audio-spectrum)
    add_test(NAME lunadash-audio-spectrum COMMAND lunadash-audio-spectrum-test)
    find_package(X11 QUIET)
    find_package(OpenGL QUIET)
    if(X11_FOUND AND OpenGL_FOUND)
        add_executable(lunadash-x11-notification-test tests/wayland/x11_notification_client.c)
        target_link_libraries(lunadash-x11-notification-test PRIVATE X11::X11 OpenGL::GL)
    endif()
    add_executable(lunadash-thumbnail-test tests/renderer/ThumbnailReadbackTests.cpp)
    target_compile_definitions(lunadash-thumbnail-test PRIVATE WLR_USE_UNSTABLE=1)
    target_link_libraries(lunadash-thumbnail-test PRIVATE ludash-thumbnail-readback Qt6::Test)
    add_test(NAME lunadash-thumbnail COMMAND lunadash-thumbnail-test)
    set_tests_properties(lunadash-thumbnail PROPERTIES TIMEOUT 30)
    add_executable(lunadash-scene-backdrop-test tests/renderer/SceneBackdropTests.cpp)
    target_compile_definitions(lunadash-scene-backdrop-test PRIVATE WLR_USE_UNSTABLE=1)
    target_link_libraries(lunadash-scene-backdrop-test PRIVATE ludash-scene-backdrop Qt6::Test)
    add_test(NAME lunadash-scene-backdrop COMMAND lunadash-scene-backdrop-test)
    set_tests_properties(lunadash-scene-backdrop PROPERTIES TIMEOUT 30)
    add_executable(lunadash-window-glass-test tests/renderer/WindowGlassTests.cpp)
    target_compile_definitions(lunadash-window-glass-test PRIVATE WLR_USE_UNSTABLE=1)
    target_link_libraries(lunadash-window-glass-test PRIVATE
        ludash-scene-window-effects Qt6::Test)
    add_test(NAME lunadash-window-glass COMMAND lunadash-window-glass-test)
    set_tests_properties(lunadash-window-glass PROPERTIES TIMEOUT 30)
    add_executable(lunadash-corner-geometry-test tests/renderer/CornerGeometryTests.cpp)
    target_link_libraries(lunadash-corner-geometry-test PRIVATE ludash-corner-geometry Qt6::Test)
    add_test(NAME lunadash-corner-geometry COMMAND lunadash-corner-geometry-test)
    set_tests_properties(lunadash-corner-geometry PROPERTIES TIMEOUT 30)
    add_executable(lunadash-media-test tests/media/MediaPlayerTests.cpp)
    target_link_libraries(lunadash-media-test PRIVATE Qt6::Core Qt6::DBus Qt6::Test)
    add_executable(lunadash-media-client tests/media/MediaPlayerClient.cpp)
    target_link_libraries(lunadash-media-client PRIVATE ludash-media)
    set_target_properties(lunadash-media-client PROPERTIES
        AUTOMOC OFF OUTPUT_NAME lunadash-media-client)
    add_dependencies(lunadash-media-test lunadash-media-client)
    add_test(NAME lunadash-media COMMAND lunadash-media-test)
    set_tests_properties(lunadash-media PROPERTIES TIMEOUT 30)
    add_test(NAME lunadash-clipboard-history COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/session/test_clipboard_history.py)
    set_tests_properties(lunadash-clipboard-history PROPERTIES TIMEOUT 30)
    add_executable(lunadash-launch-policy-test tests/session/LaunchPolicyTests.cpp)
    target_link_libraries(lunadash-launch-policy-test PRIVATE ludash-launch-policy Qt6::Test)
    add_test(NAME lunadash-launch-policy COMMAND lunadash-launch-policy-test)
    set_tests_properties(lunadash-launch-policy PROPERTIES TIMEOUT 30)
    add_executable(lunadash-window-animation-template-test
        tests/animation/WindowAnimationTemplateTests.cpp)
    target_include_directories(lunadash-window-animation-template-test PRIVATE src)
    target_link_libraries(lunadash-window-animation-template-test PRIVATE Qt6::Test)
    add_test(NAME lunadash-window-animation-template
        COMMAND lunadash-window-animation-template-test)
    set_tests_properties(lunadash-window-animation-template PROPERTIES TIMEOUT 30)
    add_executable(lunadash-settings-test tests/settings/SettingsTests.cpp
        src/compositor/settings/SettingsApi.cpp)
    target_link_libraries(lunadash-settings-test PRIVATE ludash-plugins ludash-shell-modules ludash-orbit-settings ludash-configuration Qt6::Test)
    target_compile_definitions(lunadash-settings-test PRIVATE
        SETTINGS_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/settings/contract.json")
    if(TARGET ludash-fade)
        add_dependencies(lunadash-settings-test ludash-fade)
    endif()
    add_test(NAME lunadash-settings COMMAND lunadash-settings-test)
    set_tests_properties(lunadash-settings PROPERTIES TIMEOUT 30)
    add_test(NAME lunadash-settings-schema COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/settings/test_schema.py)
    add_executable(lunadash-plugin-test tests/plugins/PluginTests.cpp)
    target_link_libraries(lunadash-plugin-test PRIVATE ludash-plugins Qt6::Test)
    if(TARGET ludash-fade)
        add_dependencies(lunadash-plugin-test ludash-fade lunadash-stacking-windows)
    endif()
    add_test(NAME lunadash-plugins COMMAND lunadash-plugin-test)
    set_tests_properties(lunadash-plugins PROPERTIES TIMEOUT 30)
    add_executable(lunadash-window-layout-test tests/tiling/WindowLayoutTests.cpp)
    target_link_libraries(lunadash-window-layout-test PRIVATE
        ludash-tiling ludash-window-rules ludash-shortcut-settings
        ludash-default-applications ludash-desktop-preferences Qt6::Test)
    add_test(NAME lunadash-window-layout COMMAND lunadash-window-layout-test)
    set_tests_properties(lunadash-window-layout PROPERTIES TIMEOUT 30)
endif()
