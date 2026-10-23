cmake_minimum_required(VERSION 3.21)
project(LunaDashWindowAnimationTemplate LANGUAGES C CXX)

if(NOT COMMAND lunadash_add_plugin)
    find_package(LunaDashPlugin 2 CONFIG REQUIRED)
endif()

lunadash_require_nlohmann_json()
lunadash_add_plugin(lunadash-window-animation-template
    METADATA "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json"
    SOURCES WindowAnimation.cpp)
target_link_libraries(lunadash-window-animation-template PRIVATE nlohmann_json::nlohmann_json)
