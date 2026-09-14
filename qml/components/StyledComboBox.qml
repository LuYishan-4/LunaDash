import QtQuick
import QtQuick.Controls
import "../style"

ComboBox {
    id: control
    implicitWidth: Math.max(150, contentItem.implicitWidth + 54)
    implicitHeight: 40
    leftPadding: 14
    rightPadding: 38
    font.family: Theme.font
    font.pixelSize: 12

    contentItem: Text {
        leftPadding: control.leftPadding
        rightPadding: control.rightPadding
        text: control.displayText
        color: Theme.text
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Item {
        width: 30
        height: control.height
        anchors.right: parent.right
        LineIcon {
            anchors.centerIn: parent
            width: 15
            height: 15
            name: "chevronDown"
            ink: control.popup.visible ? Theme.accent : Theme.muted
            rotation: control.popup.visible ? 180 : 0
            Behavior on rotation { NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }
        }
    }

    background: Rectangle {
        radius: 12
        color: control.down || control.popup.visible
            ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
            : control.hovered ? "#334353" : "#25313e"
        border.width: control.activeFocus || control.popup.visible ? 1.5 : 1
        border.color: control.activeFocus || control.popup.visible ? Theme.accent : Theme.border
        Behavior on color { ColorAnimation { duration: Theme.motion } }
        Behavior on border.color { ColorAnimation { duration: Theme.motion } }
    }

    delegate: ItemDelegate {
        // Qt 6 no longer exposes the delegate's index as an implicit context
        // property. Without the required declaration every opened popup logs
        // "ReferenceError: index is not defined" once per item.
        required property var modelData
        required property int index
        width: control.popup.width - 12
        height: 38
        leftPadding: 12
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            text: control.textRole.length > 0 && modelData && modelData[control.textRole] !== undefined
                ? modelData[control.textRole] : String(modelData)
            color: parent.highlighted ? Theme.accent : Theme.text
            font.family: Theme.font
            font.pixelSize: 12
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: 9
            color: parent.highlighted ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14) : "transparent"
        }
    }

    popup: Popup {
        y: control.height + 6
        width: Math.max(control.width, 180)
        implicitHeight: Math.min(contentItem.implicitHeight + 12, 320)
        padding: 6
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
        background: Rectangle {
            radius: 14
            color: "#f21c252e"
            border.width: 1
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.32)
        }
        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.motion }
                NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: Theme.motion; easing.type: Easing.OutCubic }
            }
        }
        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Math.min(Theme.motion, 100) } }
    }
}
