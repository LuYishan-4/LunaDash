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

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
    add_executable(lunadash-thumbnail-test tests/renderer/ThumbnailReadbackTests.cpp)
    target_compile_definitions(lunadash-thumbnail-test PRIVATE WLR_USE_UNSTABLE=1)
    target_link_libraries(lunadash-thumbnail-test PRIVATE ludash-thumbnail-readback Qt6::Test)
    add_test(NAME lunadash-thumbnail COMMAND lunadash-thumbnail-test)
    set_tests_properties(lunadash-thumbnail PROPERTIES TIMEOUT 30)
    add_executable(lunadash-window-layout-test tests/tiling/WindowLayoutTests.cpp)
    target_link_libraries(lunadash-window-layout-test PRIVATE
        ludash-tiling ludash-window-rules ludash-shortcut-settings ludash-default-applications Qt6::Test)
    add_test(NAME lunadash-window-layout COMMAND lunadash-window-layout-test)
    set_tests_properties(lunadash-window-layout PROPERTIES TIMEOUT 30)
endif()
