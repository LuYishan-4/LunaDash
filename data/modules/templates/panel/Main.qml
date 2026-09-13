import QtQuick
import QtQuick.Layouts
Item {
    id: root
    required property var shell
    required property var style
    required property string moduleId
    Rectangle { anchors.fill: parent; color: root.style.background; radius: root.style.radius }
    RowLayout {
        anchors.fill: parent; anchors.leftMargin: 18; anchors.rightMargin: 18; spacing: 14
        Text { text: "LuDash"; color: root.style.accent; font.pixelSize: root.style.fontSize; MouseArea { anchors.fill: parent; onClicked: root.shell.launcherOpen = !root.shell.launcherOpen } }
        Repeater {
            model: (root.shell.state.appearance || {}).workspaceCount || 4
            Text { required property int index; text: String(index + 1); color: root.shell.state.workspace === index ? root.style.accent : root.style.foreground; font.pixelSize: root.style.fontSize; MouseArea { anchors.fill: parent; onClicked: root.shell.command("workspace", index) } }
        }
        Item { Layout.fillWidth: true }
        Text { text: root.shell.focusedTitle; color: root.style.foreground; elide: Text.ElideRight; Layout.maximumWidth: 300 }
        Item { Layout.fillWidth: true }
        Text { text: "Settings"; color: root.style.accent; font.pixelSize: root.style.fontSize; MouseArea { anchors.fill: parent; onClicked: root.shell.settingsOpen = !root.shell.settingsOpen } }
    }
}
