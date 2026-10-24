cmake_minimum_required(VERSION 3.21)
project(LunaDashLocalizationTests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(GNUInstallDirs)

find_package(nlohmann_json 3.11 CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LUDASH_GIO REQUIRED IMPORTED_TARGET gio-2.0)
get_filename_component(REPOSITORY_ROOT ${CMAKE_CURRENT_LIST_DIR}/../.. ABSOLUTE)

add_library(ludash-desktop-preferences
    ${REPOSITORY_ROOT}/src/config/desktop/DesktopPreferenceStore.cpp)
target_include_directories(ludash-desktop-preferences PUBLIC ${REPOSITORY_ROOT}/src)
target_link_libraries(ludash-desktop-preferences PUBLIC nlohmann_json::nlohmann_json)

add_library(ludash-localization
    ${REPOSITORY_ROOT}/src/config/localization/Localization.cpp)
target_include_directories(ludash-localization PUBLIC ${REPOSITORY_ROOT}/src)
target_link_libraries(ludash-localization
    PUBLIC ludash-desktop-preferences nlohmann_json::nlohmann_json
    PRIVATE PkgConfig::LUDASH_GIO)
target_compile_definitions(ludash-localization PRIVATE
    LUDASH_TRANSLATIONS_SOURCE_DIR="${REPOSITORY_ROOT}/data/translations"
    LUDASH_TRANSLATIONS_INSTALL_DIR="${CMAKE_INSTALL_FULL_DATADIR}/ludash/translations")

add_executable(localization-test main.cpp)
target_link_libraries(localization-test PRIVATE ludash-localization)
enable_testing()
add_test(NAME localization-contract
    COMMAND localization-test ${REPOSITORY_ROOT}/data/translations)
