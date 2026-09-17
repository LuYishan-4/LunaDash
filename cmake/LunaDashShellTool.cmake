add_executable(ludash-shell-tool src/entrypoints/shell_tool_main.cpp)
target_link_libraries(ludash-shell-tool PRIVATE Qt6::Core Qt6::DBus)
set_target_properties(ludash-shell-tool PROPERTIES OUTPUT_NAME lunadash-shell-tool)
install(TARGETS ludash-shell-tool RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
