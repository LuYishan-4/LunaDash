import QtQuick
import QtQuick.Layouts
Item {
    id: root
    required property var shell
    required property var style
    required property string moduleId
    readonly property bool vertical: style.edge === "left" || style.edge === "right"

    Rectangle { anchors.fill: parent; color: root.style.background; radius: root.style.radius }

    Loader {
        anchors.fill: parent
        sourceComponent: root.vertical ? verticalPanel : horizontalPanel
    }

    Component {
        id: horizontalPanel
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            spacing: 14
            Text {
                text: "LunaDash"; color: root.style.accent; font.pixelSize: root.style.fontSize
                MouseArea { anchors.fill: parent; onClicked: root.shell.launcherOpen = !root.shell.launcherOpen }
            }
            Repeater {
                model: (root.shell.state.appearance || {}).workspaceCount || 4
                Text {
                    required property int index
                    text: String(index + 1)
                    color: root.shell.state.workspace === index ? root.style.accent : root.style.foreground
                    font.pixelSize: root.style.fontSize
                    MouseArea { anchors.fill: parent; onClicked: root.shell.command("workspace", index) }
                }
            }
            Item { Layout.fillWidth: true }
            Text { text: root.shell.focusedTitle; color: root.style.foreground; elide: Text.ElideRight; Layout.maximumWidth: 300 }
            Item { Layout.fillWidth: true }
            Text {
                text: "Settings"; color: root.style.accent; font.pixelSize: root.style.fontSize
                MouseArea { anchors.fill: parent; onClicked: root.shell.settingsOpen = !root.shell.settingsOpen }
            }
        }
    }

    Component {
        id: verticalPanel
        ColumnLayout {
            anchors.fill: parent
            anchors.topMargin: 12
            anchors.bottomMargin: 12
            spacing: 8
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "L"
                color: root.style.accent
                font.pixelSize: Math.max(16, root.style.fontSize + 3)
                font.bold: true
                MouseArea { anchors.fill: parent; onClicked: root.shell.launcherOpen = !root.shell.launcherOpen }
            }
            Repeater {
                model: (root.shell.state.appearance || {}).workspaceCount || 4
                Text {
                    required property int index
                    Layout.alignment: Qt.AlignHCenter
                    text: String(index + 1)
                    color: root.shell.state.workspace === index ? root.style.accent : root.style.foreground
                    font.pixelSize: root.style.fontSize
                    MouseArea { anchors.fill: parent; onClicked: root.shell.command("workspace", index) }
                }
            }
            Item { Layout.fillHeight: true }
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "⚙"
                color: root.style.accent
                font.pixelSize: root.style.fontSize + 2
                MouseArea { anchors.fill: parent; onClicked: root.shell.settingsOpen = !root.shell.settingsOpen }
            }
        }
    }
}
