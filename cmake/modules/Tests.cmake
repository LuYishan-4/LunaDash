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
