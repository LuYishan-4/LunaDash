import QtQuick
import QtQuick.Layouts
import "../../style"
Text {
    required property var shell
    property string title: ""
    text: shell.tr(title); color: Theme.accent; font.family: Theme.font; font.pixelSize: 22
    wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.bottomMargin: 8
}
