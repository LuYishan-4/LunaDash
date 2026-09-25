cmake_minimum_required(VERSION 3.21)
project(motion LANGUAGES C CXX)
find_package(LunaDashPlugin 2 CONFIG REQUIRED)
lunadash_add_plugin(motion METADATA "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json" SOURCES Effect.c)
