# Desktop-entry parsing, field-code expansion and MIME defaults belong to GIO.
# Avoid launching a raw Exec string through a shell.
pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-unix-2.0)
add_library(ludash-file-associations src/file_associations/FileAssociations.cpp)
target_include_directories(ludash-file-associations PUBLIC include)
target_link_libraries(ludash-file-associations PUBLIC ludash-localization Qt6::Core PRIVATE PkgConfig::GIO)
target_sources(ludash-apps PRIVATE
    src/file_association_ui/FileAssociationUi.cpp
    src/file_association_ui/LegacyFileAssociations.cpp
    src/file_manager_actions/FileManagerActions.cpp
)
target_link_libraries(ludash-apps PUBLIC ludash-file-associations)
option(LUDASH_BUILD_FILES_TESTS "Build file manager and association regression tests" OFF)
if(LUDASH_BUILD_FILES_TESTS)
    enable_testing()
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
    add_executable(lunadash-files-test tests/files/FileManagerTests.cpp)
    target_link_libraries(lunadash-files-test PRIVATE ludash-apps Qt6::Test)
    add_test(NAME lunadash-files COMMAND lunadash-files-test)
    set_tests_properties(lunadash-files PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
endif()
