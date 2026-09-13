import QtQuick
Rectangle {
    id: root
    property string text: ""
    property bool active: false
    signal clicked()
    implicitWidth: Math.max(38, label.implicitWidth + 26)
    implicitHeight: 34
    radius: 8
    color: mouse.containsMouse ? "#465576" : active ? "#617ea7" : "#29354b"
    Text { id: label; anchors.centerIn: parent; text: root.text; color: "#e5edfa"; font.pixelSize: 13 }
    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; onClicked: root.clicked() }
}
