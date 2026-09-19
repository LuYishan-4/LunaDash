option(LUDASH_BUILD_RENDERER_TESTS "Build embedded shader and GL lifetime regression tests" OFF)
if(LUDASH_BUILD_RENDERER_TESTS)
    enable_testing()
    add_executable(lunadash-renderer-test tests/renderer/RendererTests.cpp)
    target_link_libraries(lunadash-renderer-test PRIVATE ludash-renderer)
    add_test(NAME lunadash-renderer COMMAND lunadash-renderer-test)
    set_tests_properties(lunadash-renderer PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)
    # Explicit test component for staged-install relocation verification only.
    install(TARGETS lunadash-renderer-test
        RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/lunadash/tests
        COMPONENT Tests EXCLUDE_FROM_ALL)
endif()

option(LUDASH_BUILD_DESKTOP_TESTS "Build desktop controls and startup regression tests" OFF)
if(LUDASH_BUILD_DESKTOP_TESTS)
    enable_testing()
    add_executable(lunadash-desktop-controls-test
        tests/desktop/DesktopControlsTests.cpp
        src/compositor/session/ClientLaunch.cpp
        tests/desktop/DdcBrightnessTests.cpp)
    target_link_libraries(lunadash-desktop-controls-test PRIVATE
        ludash-display-settings ludash-shortcut-settings ludash-session-environment
        ludash-window-rules ludash-xwayland)
    add_test(NAME lunadash-desktop-controls COMMAND lunadash-desktop-controls-test)
    set_tests_properties(lunadash-desktop-controls PROPERTIES TIMEOUT 15)
    add_executable(lunadash-scene-animations-test
        tests/desktop/SceneWindowAnimationsTests.cpp)
    target_compile_definitions(lunadash-scene-animations-test PRIVATE WLR_USE_UNSTABLE=1)
    target_link_libraries(lunadash-scene-animations-test PRIVATE
        ludash-animation PkgConfig::WLROOTS PkgConfig::WAYLAND_SERVER)
    add_test(NAME lunadash-scene-animations COMMAND lunadash-scene-animations-test)
    set_tests_properties(lunadash-scene-animations PROPERTIES TIMEOUT 15)
    add_executable(lunadash-screen-capture-test
        tests/desktop/ScreenCaptureTests.cpp
        src/compositor/capture/ScreenCapture.cpp
        src/compositor/capture/ScreenCapture.hpp)
    target_link_libraries(lunadash-screen-capture-test PRIVATE Qt6::Core)
    target_include_directories(lunadash-screen-capture-test PRIVATE src)
    add_test(NAME lunadash-screen-capture COMMAND lunadash-screen-capture-test)
    set_tests_properties(lunadash-screen-capture PROPERTIES TIMEOUT 15)
endif()
