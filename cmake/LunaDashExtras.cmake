# Small optional install additions kept out of the main build definition so
# packaging and maintenance changes do not require rewriting LunaDashMain.cmake.
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/lunadash-update
    ${CMAKE_CURRENT_BINARY_DIR}/lunadash-update
    COPYONLY)
install(PROGRAMS scripts/lunadash-update scripts/lunadash-polkit-agent DESTINATION ${CMAKE_INSTALL_BINDIR})

# lunadash-shell-tool is defined once in LunaDashShellTool.cmake. Keeping a
# second executable target here caused Ninja to see two rules producing the
# same lunadash-shell-tool output.

add_executable(lunadash-portal src/service/portal/Main.cpp
    src/service/portal/Portal.cpp
    src/service/portal/FileChooserPortal.cpp
    src/service/portal/FileChooserPortal.hpp)
target_link_libraries(lunadash-portal PRIVATE ludash-apps Qt6::Widgets Qt6::DBus)
set_target_properties(lunadash-portal PROPERTIES OUTPUT_NAME xdg-desktop-portal-lunadash)

configure_file(
    data/portal/org.freedesktop.impl.portal.desktop.lunadash.service.in
    org.freedesktop.impl.portal.desktop.lunadash.service
    @ONLY)

install(TARGETS lunadash-portal RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
install(FILES data/portal/lunadash.portal DESTINATION ${CMAKE_INSTALL_DATADIR}/xdg-desktop-portal/portals)
install(FILES data/portal/lunadash-portals.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/xdg-desktop-portal)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.impl.portal.desktop.lunadash.service
        DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services)
