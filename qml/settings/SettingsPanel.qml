import "../modules"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../components"
import "../style"
import "components" as SettingsComponents
ModuleSurface {
    id: settings
    moduleId: "settings"
    property string category: "general"
    property var categories: [
        {id:"general", name:"General"}, {id:"appearance", name:"Appearance"}, {id:"windows", name:"Windows and workspaces"},
        {id:"modules", name:"Shell modules"}, {id:"display", name:"Display"}, {id:"input", name:"Keyboard and pointer"}, {id:"sound", name:"Sound"},
        {id:"network", name:"Network"}, {id:"bluetooth", name:"Bluetooth"}, {id:"power", name:"Power and battery"},
        {id:"applications", name:"Applications and startup"}, {id:"privacy", name:"Privacy and accessibility"},
        {id:"system", name:"Users, date and time"}, {id:"devices", name:"Printers and storage"}, {id:"about", name:"About LuDash"}
    ]
    function showCategory(id) { category = id; pageLoader.setSource(Qt.resolvedUrl("pages/" + id + ".qml"), {shell: settings.shell}) }
    anchors { top: true }
    margins.top: Theme.barHeight + moduleMargin
    implicitWidth: moduleWidth(screen ? Math.min(1080, screen.width - 36) : 1080)
    implicitHeight: moduleHeight(screen ? Math.min(740, screen.height - Theme.barHeight - 32) : 740)
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "ludash-settings"
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.Exclusive
    color: "transparent"
    Rectangle { anchors.fill: parent; color: moduleBackground; border.color: Theme.border; radius: moduleRadius }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 24; spacing: 20
        RowLayout {
            SettingsComponents.PageTitle { shell: settings.shell; title: "Settings"; color: moduleForeground; font.pixelSize: 23; Layout.fillWidth: true }
            Text { text: "LuDash"; color: moduleAccent; font.pixelSize: 12 }
            ShellButton { text: "×"; Accessible.name: shell.tr("Close settings"); onClicked: shell.settingsOpen = false }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 24
            ColumnLayout {
                Layout.preferredWidth: 238; Layout.minimumWidth: 238; Layout.maximumWidth: 238; Layout.fillHeight: true; spacing: 12
                TextField {
                    id: search; Layout.fillWidth: true; implicitHeight: 38; leftPadding: 36; placeholderText: shell.tr("Search settings"); placeholderTextColor: Theme.muted
                    color: moduleForeground; font.family: Theme.font
                    background: Rectangle { color: Theme.surface; radius: 12; border.color: search.activeFocus ? Theme.accent : Theme.border }
                    LineIcon { name: "search"; width: 17; height: 17; anchors.left: parent.left; anchors.leftMargin: 11; anchors.verticalCenter: parent.verticalCenter }
                    Keys.onEscapePressed: shell.settingsOpen = false
                }
                ListView {
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 3
                    model: settings.categories.filter(entry => shell.tr(entry.name).toLowerCase().includes(search.text.toLowerCase()))
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        id: categoryRow
                        required property var modelData
                        readonly property bool selected: settings.category === modelData.id
                        width: ListView.view.width - 12; height: 38; radius: 10
                        color: selected ? Qt.rgba(settings.moduleAccent.r, settings.moduleAccent.g, settings.moduleAccent.b, 0.14) : categoryMouse.containsMouse ? "#263340" : "transparent"
                        activeFocusOnTab: true; border.width: activeFocus ? 1 : 0; border.color: settings.moduleAccent
                        Accessible.role: Accessible.Button; Accessible.name: shell.tr(modelData.name)
                        Keys.onReturnPressed: settings.showCategory(modelData.id)
                        Keys.onSpacePressed: settings.showCategory(modelData.id)
                        Row {
                            anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter; spacing: 12
                            LineIcon { name: modelData.id; width: 19; height: 19; ink: categoryRow.selected ? settings.moduleAccent : Theme.muted }
                            Text { text: shell.tr(modelData.name); color: categoryRow.selected ? settings.moduleAccent : Theme.text; font.pixelSize: 12; font.family: Theme.font; anchors.verticalCenter: parent.verticalCenter }
                        }
                        Rectangle { visible: categoryRow.selected; width: 3; height: 15; radius: 1.5; color: settings.moduleAccent; anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter }
                        MouseArea { id: categoryMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: settings.showCategory(modelData.id) }
                        Behavior on color { ColorAnimation { duration: Theme.motion } }
                    }
                }
            }
            Rectangle { Layout.fillHeight: true; width: 1; color: Theme.border }
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumWidth: 300; radius: 18; color: "#b51c2631"
                ScrollView {
                    id: scroll; anchors.fill: parent; anchors.margins: 24; clip: true
                    contentWidth: availableWidth
                    Loader { id: pageLoader; width: scroll.availableWidth - 12; onLoaded: { scroll.contentItem.contentY = 0; if (Quickshell.env("LUDASH_TEST_SETTINGS") === "1") console.info("Settings page loaded: " + settings.category) } }
                }
            }
        }
    }
    Component.onCompleted: showCategory("general")
}
