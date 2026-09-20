import "../modules"
import QtQuick
import Quickshell
import Quickshell.Wayland
import "../components"
import "../style"

// Desktop context menu. The surface spans the screen but is only mapped while
// open, so it never blocks pointer input at rest. The card grows from the click
// point instead of appearing over it.
ModuleSurface {
    id: menu
    moduleId: "menu"
    extensionTarget: "desktop-menu"
    property real anchorX: 0
    property real anchorY: 0
    readonly property bool hasFocusedClient: (shell.state.clients || []).some(client => client.focused && !client.desktop)

    anchors { top: true; bottom: true; left: true; right: true }
    margins { top: moduleMargin; bottom: moduleMargin; left: moduleMargin; right: moduleMargin }
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "lunadash-menu"
    WlrLayershell.keyboardFocus: opened ? WlrKeyboardFocus.Exclusive : WlrKeyboardFocus.None
    color: "transparent"

    function close() { menu.shell.menuOpen = false }
    function run(method, value) { close(); menu.shell.command(method, value) }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        onClicked: menu.close()
    }

    Rectangle {
        id: card
        readonly property int cardWidth: 216
        readonly property int cardHeight: column.implicitHeight + 16

        width: cardWidth
        height: cardHeight
        x: Math.max(6, Math.min(menu.anchorX, Math.max(6, menu.width - width - 6)))
        y: Math.max(6, Math.min(menu.anchorY, Math.max(6, menu.height - height - 6)))
        radius: 14
        color: moduleBackground
        border.width: 1
        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.26)
        opacity: menu.reveal
        scale: 0.9 + 0.1 * menu.reveal
        transformOrigin: Item.TopLeft

        focus: true
        Keys.onEscapePressed: menu.close()

        Column {
            id: column
            anchors.fill: parent
            anchors.margins: 8
            spacing: 2

            MenuEntry {
                width: column.width
                text: shell.tr("Copy")
                glyph: "copy"
                available: menu.hasFocusedClient
                onTriggered: menu.run("send-key", "copy")
            }
            MenuEntry {
                width: column.width
                text: shell.tr("Paste")
                glyph: "paste"
                available: menu.hasFocusedClient
                onTriggered: menu.run("send-key", "paste")
            }
            Rectangle { width: column.width; height: 1; color: Theme.border }
            MenuEntry {
                width: column.width
                text: shell.tr("Open terminal")
                glyph: "terminal"
                onTriggered: { menu.close(); menu.shell.launch("terminal") }
            }
            MenuEntry {
                width: column.width
                text: shell.tr("Settings")
                glyph: "settings"
                onTriggered: { menu.close(); menu.shell.settingsOpen = true }
            }
            Rectangle { width: column.width; height: 1; color: Theme.border }
            MenuEntry {
                width: column.width
                text: shell.tr("About LunaDash")
                glyph: "about"
                onTriggered: menu.run("open-settings", "about")
            }
        }
    }
}
