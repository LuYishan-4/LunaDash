cmake_minimum_required(VERSION 3.21)
project(LunaDash VERSION 0.1.0 LANGUAGES C CXX)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/LunaDashMain.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/LunaDashShellTool.cmake)

# Native input is an optional compiled capability. Runtime selection is handled
# by lunadash-compositor --nested, so one binary can serve both installed and
# nested development sessions.
option(LUDASH_USE_LIBINPUT "Compile the native libinput keyboard backend" ON)
if(LUDASH_USE_LIBINPUT)
    pkg_check_modules(LIBINPUT REQUIRED IMPORTED_TARGET libinput)
    pkg_check_modules(LIBUDEV REQUIRED IMPORTED_TARGET libudev)
    pkg_check_modules(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)
    target_compile_definitions(ludash-input-settings PRIVATE LUDASH_USE_LIBINPUT=1)
    target_link_libraries(ludash-input-settings PRIVATE PkgConfig::LIBINPUT PkgConfig::LIBUDEV PkgConfig::XKBCOMMON)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/cmake/LunaDashExtras.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/LunaDashFiles.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/LunaDashTranslations.cmake)
