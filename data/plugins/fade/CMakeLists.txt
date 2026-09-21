cmake_minimum_required(VERSION 3.21)
project(lunadash_fade VERSION 2.0.0 LANGUAGES C CXX)

# A real SDK 2 plugin package can build standalone against an installed/build-tree
# LunaDash SDK, while the LunaDash source tree may provide the command directly.
if(NOT COMMAND lunadash_add_plugin)
    find_package(LunaDashPlugin 2 CONFIG REQUIRED)
endif()
find_package(Qt6 6.4 REQUIRED COMPONENTS Core)

lunadash_add_plugin(lunadash-fade
    METADATA "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json"
    SOURCES Effect.cpp)
target_link_libraries(lunadash-fade PRIVATE Qt6::Core)
