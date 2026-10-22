import QtQuick
import QtQuick.Layouts
import "../style"

Rectangle {
    id: card
    default property alias content: body.data
    Layout.fillWidth: true
    Layout.minimumWidth: 0
    implicitHeight: body.implicitHeight + 32
    color: Theme.surfaceGlass
    radius: Theme.radiusMedium
    border.width: 1
    border.color: Theme.hairline
    clip: true
    ColumnLayout {
        id: body
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12
    }
}
