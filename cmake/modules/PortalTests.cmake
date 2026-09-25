# Keep core portal regressions in the existing CTest build, without adding a
# workflow (PR policy protects .github/workflows). No display server is needed.
find_program(LUDASH_DBUS_RUN_SESSION dbus-run-session REQUIRED)
add_executable(lunadash-portal-protocol-test tests/portal/PortalProtocolTests.cpp)
target_link_libraries(lunadash-portal-protocol-test PRIVATE Qt6::Core Qt6::DBus Qt6::Test)
add_dependencies(lunadash-portal-protocol-test lunadash-portal)
add_test(NAME lunadash-portal-protocol
    COMMAND ${LUDASH_DBUS_RUN_SESSION} -- $<TARGET_FILE:lunadash-portal-protocol-test>)
set_tests_properties(lunadash-portal-protocol PROPERTIES TIMEOUT 45)
add_test(NAME lunadash-session-launcher COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/wayland/test_session_launcher.py)
set_tests_properties(lunadash-session-launcher PROPERTIES TIMEOUT 45)

# The full UI/frontend suite is available to maintainers with its additional
# system dependencies installed. Enabling it must fail, not skip, on omissions.
option(LUDASH_BUILD_PORTAL_RUNTIME_TESTS "Run the real portal frontend and Xvfb UI suite" OFF)
if(LUDASH_BUILD_PORTAL_RUNTIME_TESTS)
    find_program(LUDASH_XVFB_RUN xvfb-run REQUIRED)
    find_program(LUDASH_XAUTH xauth REQUIRED)
    find_program(LUDASH_XDOTOOL xdotool REQUIRED)
    find_program(LUDASH_PORTAL_FRONTEND xdg-desktop-portal
        PATHS /usr/libexec /usr/lib REQUIRED)
    execute_process(COMMAND ${Python3_EXECUTABLE} -c "import dbus; from gi.repository import GLib"
        RESULT_VARIABLE portal_python_status ERROR_VARIABLE portal_python_error)
    if(NOT portal_python_status EQUAL 0)
        message(FATAL_ERROR "Portal runtime tests need Python dbus and gi: ${portal_python_error}")
    endif()
    add_test(NAME lunadash-portal-runtime COMMAND sh
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/portal/run_runtime.sh
        ${CMAKE_CURRENT_BINARY_DIR} ${Python3_EXECUTABLE})
    set_tests_properties(lunadash-portal-runtime PROPERTIES TIMEOUT 120)
endif()
