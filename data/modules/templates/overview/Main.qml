import QtQuick
import QtQuick.Layouts
Item {
    id: root
    required property var shell
    required property var style
    required property string moduleId
    Rectangle { anchors.fill: parent; color: root.style.background; radius: root.style.radius }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 30; spacing: 18
        Text { text: "My LuDash dashboard"; color: root.style.accent; font.pixelSize: 28 }
        Text { text: ((root.shell.state.system || {}).cpuPercent || 0) + "% CPU"; color: root.style.foreground; font.pixelSize: 36 }
        Text { text: (root.shell.state.clients || []).length + " open windows"; color: root.style.foreground; font.pixelSize: root.style.fontSize }
        Item { Layout.fillHeight: true }
        Text { text: "Close dashboard"; color: root.style.accent; MouseArea { anchors.fill: parent; onClicked: root.shell.setAppearance({overview: false}) } }
    }
}
