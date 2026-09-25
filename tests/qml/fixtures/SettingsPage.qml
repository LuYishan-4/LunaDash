import QtQuick
import QtQuick.Controls

Item {
    required property var shell
    implicitHeight: shell.fixtureHeight
    Button {
        objectName: "pageFooter"
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        text: "Last setting"
        onClicked: parent.shell.footerClicked = true
    }
}
