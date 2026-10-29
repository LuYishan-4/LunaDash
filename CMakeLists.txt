cmake_minimum_required(VERSION 3.21)
set(LUDASH_RELEASE_VERSION "1.0.2a")
project(LunaDash VERSION 1.0.2 LANGUAGES C CXX)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
    set(LUDASH_FETCHCONTENT_TIMESTAMP DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
endif()

# The session shell is the external LunaDash-Shell repository: Slint files run by SlintLu
# (shell.slint).
# LUDASH_QML_SOURCE_DIR is the former name of this setting and still works.
set(LUDASH_SHELL_DEFAULT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shell")
if(DEFINED LUDASH_QML_SOURCE_DIR AND NOT DEFINED LUDASH_SHELL_SOURCE_DIR)
    set(LUDASH_SHELL_DEFAULT_DIR "${LUDASH_QML_SOURCE_DIR}")
endif()
set(LUDASH_SHELL_SOURCE_DIR "${LUDASH_SHELL_DEFAULT_DIR}" CACHE PATH
    "External LunaDash-Shell checkout used for development and installation")
# OFF builds the compositor for a custom shell or none (session.json): no shell files and no
# SlintLu are built or installed.
option(LUDASH_INSTALL_SHELL "Build and install the LunaDash shell and SlintLu" ON)
if(NOT LUDASH_INSTALL_SHELL)
    set(LUDASH_SHELL_SOURCE_DIR "" CACHE PATH "" FORCE)
    set(LUDASH_SLINTLU_SOURCE_DIR "" CACHE PATH "" FORCE)
elseif(NOT EXISTS "${LUDASH_SHELL_SOURCE_DIR}/shell.slint")
    message(FATAL_ERROR
        "LunaDash-Shell source is required. Run scripts/fetch-shell.sh <directory> and configure with -DLUDASH_SHELL_SOURCE_DIR=<directory>.")
endif()

# LunaDash-owned Wayland UI clients use Slint. Compile it from its pinned
# source so backend selection is explicit and never picks Qt because Qt is
# present on the build host.
# Packagers may build Slint once (with the SLINT_FEATURE values below) and pass -DLUDASH_SYSTEM_SLINT=ON;
# the Nix package does, so its binary cache keeps Slint across LunaDash changes.
option(LUDASH_SYSTEM_SLINT "Use an installed Slint C++ package instead of building Slint" OFF)
if(NOT LUDASH_SYSTEM_SLINT)
    find_program(LUDASH_RUSTC rustc REQUIRED)
    find_program(LUDASH_CARGO cargo REQUIRED)
    execute_process(COMMAND "${LUDASH_RUSTC}" --version
        OUTPUT_VARIABLE LUDASH_RUST_VERSION_TEXT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE LUDASH_RUST_VERSION_RESULT)
    if(NOT LUDASH_RUST_VERSION_RESULT EQUAL 0 OR
       NOT LUDASH_RUST_VERSION_TEXT MATCHES "rustc ([0-9]+\\.[0-9]+)")
        message(FATAL_ERROR "Could not determine the Rust compiler version required by Slint")
    endif()
    if(CMAKE_MATCH_1 VERSION_LESS "1.92")
        message(FATAL_ERROR "Slint requires Rust 1.92 or newer; found ${LUDASH_RUST_VERSION_TEXT}")
    endif()
endif()
set(SLINT_FEATURE_BACKEND_QT OFF CACHE BOOL "Do not include Slint's Qt backend" FORCE)
set(SLINT_FEATURE_BACKEND_WINIT OFF CACHE BOOL "Use only Slint's Wayland Winit backend" FORCE)
set(SLINT_FEATURE_BACKEND_WINIT_WAYLAND ON CACHE BOOL "Enable Slint's Wayland client backend" FORCE)
set(SLINT_FEATURE_BACKEND_WINIT_X11 OFF CACHE BOOL "Do not include Slint's X11 backend" FORCE)
set(SLINT_FEATURE_RENDERER_FEMTOVG ON CACHE BOOL "Use Slint's OpenGL renderer" FORCE)
set(SLINT_FEATURE_RENDERER_SOFTWARE OFF CACHE BOOL "Avoid a renderer with limited CJK text support" FORCE)
set(SLINT_FEATURE_INTERPRETER OFF CACHE BOOL "Use ahead-of-time compiled Welcome UI" FORCE)
set(SLINT_FEATURE_SYSTEM_TRAY OFF CACHE BOOL "Welcome does not use system tray icons" FORCE)
set(SLINT_FEATURE_TESTING OFF CACHE BOOL "Disable Slint's runtime testing API" FORCE)
if(LUDASH_SYSTEM_SLINT)
    find_package(Slint 1.18 CONFIG REQUIRED)
else()
    include(FetchContent)
    FetchContent_Declare(Slint
        URL https://github.com/slint-ui/slint/archive/refs/tags/v1.18.1.tar.gz
        URL_HASH SHA256=fe485305ed303215e76c04918ee9aefbffbe229f18f979098ec36c7fa1dab28b
        ${LUDASH_FETCHCONTENT_TIMESTAMP}
        SOURCE_SUBDIR api/cpp)
    FetchContent_MakeAvailable(Slint)
endif()
get_target_property(LUDASH_SLINT_FEATURES Slint::Slint SLINT_ENABLED_FEATURES)
if("BACKEND_QT" IN_LIST LUDASH_SLINT_FEATURES OR
   NOT "BACKEND_WINIT_WAYLAND" IN_LIST LUDASH_SLINT_FEATURES)
    message(FATAL_ERROR
        "Slint UI targets require Winit Wayland and forbid the Qt backend")
endif()

include(${CMAKE_CURRENT_LIST_DIR}/cmake/LunaDashMain.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/LunaDashSlintLu.cmake)

# xkbregistry is shipped separately by some distributions. Keep dynamic
# keyboard-layout discovery when it is available, but do not make the entire
# compositor unbuildable when only libxkbcommon is packaged.
pkg_check_modules(XKBREGISTRY QUIET IMPORTED_TARGET xkbregistry)
if(XKBREGISTRY_FOUND)
    target_compile_definitions(ludash-input-settings PRIVATE LUDASH_HAS_XKBREGISTRY=1)
    target_link_libraries(ludash-input-settings PRIVATE PkgConfig::XKBREGISTRY)
endif()

# Physical input is owned by wlroots. LunaDash does not open a second
# libinput context and does not route compositor input through Qt.
include(${CMAKE_CURRENT_LIST_DIR}/cmake/LunaDashExtras.cmake)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/modules/Tests.cmake)

if(BUILD_TESTING)
    include(${CMAKE_CURRENT_LIST_DIR}/cmake/modules/PortalTests.cmake)
endif()

# Slint enables BUILD_SHARED_LIBS for its runtime. Install LunaDash's shared
# components as well so installed executables do not retain build-tree-only
# dependencies. Static builds need no separate component installation.
get_property(LUDASH_DIRECTORY_TARGETS DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(LUDASH_TARGET IN LISTS LUDASH_DIRECTORY_TARGETS)
    get_target_property(LUDASH_TARGET_TYPE ${LUDASH_TARGET} TYPE)
    if(LUDASH_TARGET_TYPE STREQUAL "SHARED_LIBRARY")
        install(TARGETS ${LUDASH_TARGET}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT Runtime)
        set_property(TARGET ${LUDASH_TARGET} PROPERTY INSTALL_RPATH "$ORIGIN")
    endif()
endforeach()

foreach(LUDASH_RUNTIME_TARGET IN ITEMS
        ludash-desktop ludash-welcome ludash-packages ludash-compositor
        ludashctl ludash-screencast-chooser ludash-filepicker
        lunadash-portal)
    if(TARGET ${LUDASH_RUNTIME_TARGET})
        set_property(TARGET ${LUDASH_RUNTIME_TARGET} PROPERTY INSTALL_RPATH
            "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
    endif()
endforeach()

if(TARGET lunadash-renderer-test)
    set_property(TARGET lunadash-renderer-test PROPERTY INSTALL_RPATH
        "$ORIGIN/../../../${CMAKE_INSTALL_LIBDIR}")
endif()
