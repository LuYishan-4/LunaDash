import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"

ComboBox {
    id: control
    property var translationContext: null
    property bool popupArmed: false
    function translated(text) {
        return translationContext ? translationContext.tr(String(text)) : String(text)
    }
    implicitWidth: Math.max(150, contentItem.implicitWidth + 54)
    Layout.minimumWidth: 120
    implicitHeight: Math.max(40, contentItem.implicitHeight + topPadding + bottomPadding)
    leftPadding: 14
    rightPadding: 38
    font.family: Theme.font
    font.pixelSize: 12
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    scale: down ? 0.985 : hovered ? 1.008 : 1

    contentItem: Text {
        leftPadding: control.leftPadding
        rightPadding: control.rightPadding
        text: control.translated(control.displayText)
        color: Theme.text
        font: control.font
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.Wrap
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
        id: option
        required property int index
        width: control.popup.width - 12
        implicitHeight: Math.max(38, option.contentItem.implicitHeight + topPadding + bottomPadding)
        leftPadding: 12
        enabled: control.popupArmed
        highlighted: control.highlightedIndex === index
        text: control.translated(control.textAt(index))
        Accessible.name: text
        contentItem: Row {
            spacing: 8
            LineIcon {
                visible: option.index === control.currentIndex
                width: visible ? 14 : 0
                height: 14
                anchors.verticalCenter: parent.verticalCenter
                name: "check"
                ink: Theme.accent
            }
            Text {
                width: Math.max(0, parent.width - (option.index === control.currentIndex ? 22 : 0))
                text: option.text
            color: option.highlighted ? Theme.accent : Theme.text
            font.family: Theme.font
            font.pixelSize: 12
            verticalAlignment: Text.AlignVCenter
                wrapMode: Text.Wrap
            }
        }
        background: Rectangle {
            radius: 9
            color: option.highlighted ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14) : "transparent"
        }
    }

    Behavior on scale { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }

    popup: Popup {
        id: menuPopup
        y: control.height + 6
        width: Math.max(control.width, 180)
        implicitHeight: Math.min(menuList.contentHeight + 12, 320)
        padding: 6
        modal: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onOpened: {
            control.popupArmed = false
            armTimer.restart()
            Qt.callLater(function() {
                if (control.currentIndex >= 0)
                    menuList.positionViewAtIndex(control.currentIndex, ListView.Contain)
            })
        }
        onClosed: {
            armTimer.stop()
            control.popupArmed = false
            control.forceActiveFocus()
        }

        Timer {
            id: armTimer
            interval: 140
            onTriggered: control.popupArmed = true
        }

        contentItem: ListView {
            id: menuList
            clip: true
            implicitHeight: contentHeight
            model: control.delegateModel
            currentIndex: control.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds
            flickDeceleration: 7000
            maximumFlickVelocity: 4200
            keyNavigationWraps: true
            Keys.onEscapePressed: menuPopup.close()
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
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
        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Math.min(Theme.motion, 120) } }
    }
}
