import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../components"
import "../style"
AnimatedPanel {
    id: settings
    required property var shell
    property string category: "general"
    property var categories: [
        {id:"general", name:"General"}, {id:"appearance", name:"Appearance"}, {id:"windows", name:"Windows and workspaces"},
        {id:"display", name:"Display"}, {id:"input", name:"Keyboard and pointer"}, {id:"sound", name:"Sound"},
        {id:"network", name:"Network"}, {id:"bluetooth", name:"Bluetooth"}, {id:"power", name:"Power and battery"},
        {id:"applications", name:"Applications and startup"}, {id:"privacy", name:"Privacy and accessibility"},
        {id:"system", name:"Users, date and time"}, {id:"devices", name:"Printers and storage"}, {id:"about", name:"About LuDash"}
    ]
    function showCategory(id) { category = id; pageLoader.setSource(Qt.resolvedUrl("pages/" + id + ".qml"), {shell: settings.shell}) }
    anchors { top: true }
    margins.top: Theme.barHeight + 14
    implicitWidth: screen ? Math.min(1080, screen.width - 36) : 1080
    implicitHeight: screen ? Math.min(740, screen.height - Theme.barHeight - 32) : 740
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "ludash-settings"
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.Exclusive
    color: "transparent"
    Rectangle { anchors.fill: parent; color: Theme.background; border.color: Theme.border; radius: Theme.radius }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 24; spacing: 18
        RowLayout {
            Text { text: shell.tr("Settings"); color: Theme.text; font.family: Theme.font; font.pixelSize: 26; Layout.fillWidth: true }
            Text { text: "LuDash"; color: Theme.accent; font.pixelSize: 14 }
            ShellButton { text: "×"; Accessible.name: shell.tr("Close settings"); onClicked: shell.settingsOpen = false }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 24
            ColumnLayout {
                Layout.preferredWidth: 238; Layout.fillHeight: true; spacing: 12
                TextField {
                    id: search; Layout.fillWidth: true; placeholderText: shell.tr("Search settings")
                    color: Theme.text; font.family: Theme.font
                    background: Rectangle { color: Theme.surface; radius: 12; border.color: search.activeFocus ? Theme.accent : Theme.border }
                    Keys.onEscapePressed: shell.settingsOpen = false
                }
                ListView {
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 7
                    model: settings.categories.filter(entry => shell.tr(entry.name).toLowerCase().includes(search.text.toLowerCase()))
                    ScrollBar.vertical: ScrollBar {}
                    delegate: ShellButton {
                        required property var modelData
                        width: ListView.view.width - 12; text: shell.tr(modelData.name); active: settings.category === modelData.id
                        onClicked: settings.showCategory(modelData.id)
                    }
                }
            }
            Rectangle { Layout.fillHeight: true; width: 1; color: Theme.border }
            ScrollView {
                id: scroll; Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                contentWidth: availableWidth
                Loader { id: pageLoader; width: scroll.availableWidth - 12; onLoaded: { scroll.contentItem.contentY = 0; if (Quickshell.env("LUDASH_TEST_SETTINGS") === "1") console.info("Settings page loaded: " + settings.category) } }
            }
        }
    }
    Component.onCompleted: showCategory("general")
}
