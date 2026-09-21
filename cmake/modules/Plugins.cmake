# Both in-tree examples and third-party projects use the same SDK contract.
set(LUNADASH_PLUGIN_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/src")
set(LUNADASH_PLUGIN_API_SOURCE "${CMAKE_SOURCE_DIR}/src/core/plugins/PluginApi.c")
set(LUNADASH_PLUGIN_TARGETS "${CMAKE_SOURCE_DIR}/data/plugins/targets.json")
include("${CMAKE_SOURCE_DIR}/cmake/plugins/LunaDashPlugin.cmake")
include(CMakePackageConfigHelpers)
set(sdk_install_dir "${CMAKE_INSTALL_LIBDIR}/cmake/LunaDashPlugin")
configure_package_config_file(cmake/plugins/LunaDashPluginConfig.cmake.in
    "${CMAKE_BINARY_DIR}/sdk/LunaDashPluginConfig.cmake" INSTALL_DESTINATION "${sdk_install_dir}")
write_basic_package_version_file("${CMAKE_BINARY_DIR}/sdk/LunaDashPluginConfigVersion.cmake"
    VERSION 2.0.0 COMPATIBILITY SameMajorVersion)
# Also provide a relocatable-to-source build-tree package for plugin authors.
file(WRITE "${CMAKE_BINARY_DIR}/sdk-build/LunaDashPluginConfig.cmake"
    "set(LUNADASH_PLUGIN_INCLUDE_DIR \"${LUNADASH_PLUGIN_INCLUDE_DIR}\")\nset(LUNADASH_PLUGIN_API_SOURCE \"${LUNADASH_PLUGIN_API_SOURCE}\")\nset(LUNADASH_PLUGIN_TARGETS \"${LUNADASH_PLUGIN_TARGETS}\")\ninclude(\"${CMAKE_SOURCE_DIR}/cmake/plugins/LunaDashPlugin.cmake\")\n")
configure_file("${CMAKE_BINARY_DIR}/sdk/LunaDashPluginConfigVersion.cmake"
    "${CMAKE_BINARY_DIR}/sdk-build/LunaDashPluginConfigVersion.cmake" COPYONLY)
install(FILES "${CMAKE_BINARY_DIR}/sdk/LunaDashPluginConfig.cmake"
    "${CMAKE_BINARY_DIR}/sdk/LunaDashPluginConfigVersion.cmake"
    cmake/plugins/LunaDashPlugin.cmake cmake/plugins/ValidatePlugin.py data/plugins/targets.json
    DESTINATION "${sdk_install_dir}")
install(FILES src/core/plugins/PluginApi.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/LunaDash/core/plugins")
install(FILES src/core/plugins/PluginApi.c DESTINATION "${CMAKE_INSTALL_DATADIR}/lunadash/plugin-sdk")
install(PROGRAMS scripts/create-plugin.py DESTINATION "${CMAKE_INSTALL_BINDIR}" RENAME lunadash-create-plugin)
install(FILES data/plugins/targets.json docs/PLUGIN_TARGETS.md docs/PLUGINS.md DESTINATION "${CMAKE_INSTALL_DATADIR}/lunadash/plugin-sdk")
install(DIRECTORY templates/plugins/ DESTINATION "${CMAKE_INSTALL_DATADIR}/lunadash/plugin-sdk/templates")
lunadash_add_plugin(lunadash-clock METADATA "${CMAKE_SOURCE_DIR}/qml/plugins/digital-clock/metadata.json")
if(LUDASH_BUILD_EXAMPLE_PLUGIN)
    add_subdirectory("${CMAKE_SOURCE_DIR}/data/plugins/fade" "${CMAKE_BINARY_DIR}/example-plugins/fade")
    add_subdirectory(templates/plugins/window-template)
endif()
