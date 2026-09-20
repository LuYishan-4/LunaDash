cmake_minimum_required(VERSION 3.21)
project(tint LANGUAGES C CXX)
find_package(LunaDashPlugin 2 CONFIG REQUIRED)
lunadash_add_plugin(tint METADATA "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json")
