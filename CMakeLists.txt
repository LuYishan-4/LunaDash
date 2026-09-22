cmake_minimum_required(VERSION 3.21)
set(LUDASH_RELEASE_VERSION "1.0.1a")
project(LunaDash VERSION 1.0.1 LANGUAGES C CXX)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/LunaDashMain.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/LunaDashShellTool.cmake)

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
include(${CMAKE_CURRENT_LIST_DIR}/cmake/LunaDashFiles.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/LunaDashTranslations.cmake)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/modules/Tests.cmake)
