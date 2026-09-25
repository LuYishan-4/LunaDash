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
    src/service/portal/SettingsPortal.cpp
    src/service/portal/SettingsPortal.hpp
    src/service/portal/FileChooserPortal.cpp
    src/service/portal/FileChooserPortal.hpp
    src/service/portal/FileChooserOptions.cpp
    src/service/portal/FileChooserOptions.hpp
    src/service/portal/FileIcons.cpp
    src/service/portal/FileIcons.hpp
    src/service/portal/FilePickerDialog.cpp
    src/service/portal/FilePickerDialog.hpp
    src/service/portal/ScreenCastChooser.cpp
    src/service/portal/ScreenCastChooser.hpp)
target_link_libraries(lunadash-portal PRIVATE ludash-apps Qt6::Widgets Qt6::DBus)
set_target_properties(lunadash-portal PROPERTIES OUTPUT_NAME xdg-desktop-portal-lunadash)

configure_file(
    data/portal/org.freedesktop.impl.portal.desktop.lunadash.service.in
    org.freedesktop.impl.portal.desktop.lunadash.service
    @ONLY)
configure_file(
    data/portal/xdg-desktop-portal-wlr/LunaDash.in
    xdg-desktop-portal-wlr-LunaDash
    @ONLY)

install(TARGETS lunadash-portal RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
install(FILES data/portal/lunadash.portal DESTINATION ${CMAKE_INSTALL_DATADIR}/xdg-desktop-portal/portals)
install(FILES data/portal/lunadash-portals.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/xdg-desktop-portal)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/xdg-desktop-portal-wlr-LunaDash
        DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/xdg/xdg-desktop-portal-wlr
        RENAME LunaDash)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.impl.portal.desktop.lunadash.service
        DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services)
