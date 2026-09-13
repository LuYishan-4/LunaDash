import QtQuick
import QtQuick.Layouts
import "../../style"
Text {
    required property var shell
    property string message: ""
    text: shell.tr(message); color: Theme.muted; font.family: Theme.font; font.pixelSize: 13
    wrapMode: Text.WordWrap; Layout.fillWidth: true
}
