cmake_minimum_required(VERSION 3.21)
set(LUDASH_RELEASE_VERSION "1.0.2a")
project(LunaDash VERSION 1.0.2 LANGUAGES C CXX)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
    set(LUDASH_FETCHCONTENT_TIMESTAMP DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
endif()

# ==============================================================================
# LunaDash build options
# ==============================================================================
#
# Quick start:
#   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --parallel
#   sudo cmake --install build
#
# ----- Shell -------------------------------------------------------------------
#   LUDASH_INSTALL_SHELL=ON|OFF        Build/install LunaDash-Shell and SlintLu (default: ON).
#                                      OFF builds the compositor only and installs
#                                      session.json with {"shell": "none"}.
#   LUDASH_SHELL_SOURCE_DIR=<path>     LunaDash-Shell checkout (default: ./shell).
#                                      Fetch with: scripts/fetch-shell.sh <dir>
#   LUDASH_QML_SOURCE_DIR=<path>       Legacy alias of LUDASH_SHELL_SOURCE_DIR.
#   LUDASH_SLINTLU_SOURCE_DIR=<path>   SlintLu checkout built into libexec/lunadash (optional).
#                                      Empty uses `slintlu` from PATH.
#                                      Fetch with: scripts/fetch-slintlu.sh <dir>
#
# ----- Dependencies / rendering -----------------------------------------------
#   LUDASH_ENABLE_VULKAN=ON|OFF        Enable the wlroots Vulkan renderer when available (default: ON).
#
# ----- Debug ------------------------------------------------------------------
#   LUDASH_ENABLE_SANITIZERS=ON|OFF    Build project code with ASan + UBSan (default: OFF).
#
# ----- Standard CMake ---------------------------------------------------------
#   CMAKE_BUILD_TYPE                   Debug | Release | RelWithDebInfo | MinSizeRel
#   CMAKE_INSTALL_PREFIX               Install prefix (installer uses /usr).
#   CMAKE_CXX_CLANG_TIDY=clang-tidy    Run clang-tidy during the build (also applied to C).
#
# ----- Useful commands --------------------------------------------------------
#   cmake --fresh -S . -B build                                    Reconfigure from scratch (3.24+)
#   cmake --build build --target ludash-compositor                 Build a single target
#   cmake --build build --target clean                             Clean build outputs
#   DESTDIR="$PWD/build/stage" cmake --install build --prefix /usr Staged install
#   cmake -LH build                                                List cache options with help
# ==============================================================================

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
        ludash-desktop ludash-compositor
        ludashctl
        lunadash-portal)
    if(TARGET ${LUDASH_RUNTIME_TARGET})
        set_property(TARGET ${LUDASH_RUNTIME_TARGET} PROPERTY INSTALL_RPATH
            "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
    endif()
endforeach()
