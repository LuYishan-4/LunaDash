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
            : control.hovered ? Theme.controlHover : Theme.control
        border.width: control.activeFocus || control.popup.visible ? 1.5 : 1
        border.color: control.activeFocus || control.popup.visible ? Theme.accent : Theme.border
        Behavior on color { ColorAnimation { duration: Theme.motion } }
        Behavior on border.color { ColorAnimation { duration: Theme.motion } }
    }

    delegate: ItemDelegate {
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
        id: menuPopup
        y: control.height + 6
        width: Math.max(control.width, 180)
        implicitHeight: Math.min(menuList.contentHeight + 12, 320)
        padding: 6
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onOpened: Qt.callLater(function() {
            if (control.currentIndex >= 0)
                menuList.positionViewAtIndex(control.currentIndex, ListView.Contain)
        })

        contentItem: ListView {
            id: menuList
            clip: true
            implicitHeight: contentHeight
            model: control.delegateModel
            // Do not bind ListView.currentIndex to ComboBox.highlightedIndex. A
            // rapidly moving pointer/wheel changes highlightedIndex and ListView
            // then auto-scrolls the highlighted delegate back into view, which
            // feels like the menu snapping to an older position.
            currentIndex: -1
            boundsBehavior: Flickable.StopAtBounds
            flickDeceleration: 7000
            maximumFlickVelocity: 4200
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
        }
        background: Rectangle {
            radius: 14
            color: Theme.surfaceOpaque
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
