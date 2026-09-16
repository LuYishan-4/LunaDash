# Small optional install additions kept out of the main build definition so
# packaging and maintenance changes do not require rewriting LunaDashMain.cmake.
install(PROGRAMS scripts/lunadash-update DESTINATION ${CMAKE_INSTALL_BINDIR})
