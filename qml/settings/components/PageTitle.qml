import QtQuick
import QtQuick.Layouts
import "../../style"

Text {
    id: titleText
    required property var shell
    property string title: ""

    text: shell.tr(title)
    color: Theme.accent
    font.family: Theme.font
    font.pixelSize: 22
    font.weight: Font.DemiBold
    wrapMode: Text.WordWrap
    Layout.fillWidth: true
    Layout.bottomMargin: 8
    opacity: 0
    transform: Translate { id: titleShift; x: -8 }

    Component.onCompleted: entrance.start()

    ParallelAnimation {
        id: entrance
        NumberAnimation {
            target: titleText
            property: "opacity"
            from: 0
            to: 1
            duration: Theme.motion
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: titleShift
            property: "x"
            from: -8
            to: 0
            duration: Theme.motion
            easing.type: Easing.OutCubic
        }
    }
}
