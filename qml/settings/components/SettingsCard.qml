import QtQuick
import QtQuick.Layouts
import "../../style"

Rectangle {
    id: card

    property string title: ""
    property string description: ""
    property string badge: ""
    property bool emphasized: false
    property bool interactive: false
    property real revealProgress: 0
    default property alias content: body.data

    signal activated()

    Layout.fillWidth: true
    implicitHeight: body.implicitHeight + 32
    radius: 18
    color: card.emphasized
        ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.10)
        : cardHover.hovered && card.interactive
            ? Theme.surfaceElevated
            : Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.82)
    border.width: card.emphasized || (cardHover.hovered && card.interactive) ? 1.5 : 1
    border.color: card.emphasized
        ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.52)
        : cardHover.hovered && card.interactive
            ? Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.42)
            : Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.24)
    opacity: 0.35 + 0.65 * revealProgress
    scale: cardTap.pressed ? 0.992 : cardHover.hovered && card.interactive ? 1.006 : 1
    transform: Translate {
        y: (1 - card.revealProgress) * 8
    }

    Rectangle {
        visible: card.emphasized
        width: 3
        radius: 1.5
        anchors.left: parent.left
        anchors.leftMargin: 2
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: 14
        anchors.bottomMargin: 14
        color: Theme.accent
    }

    Rectangle {
        width: 34
        height: 34
        radius: 17
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        color: Qt.rgba(Theme.moon.r, Theme.moon.g, Theme.moon.b,
                       cardHover.hovered && card.interactive ? 0.13 : 0.08)

        Rectangle {
            width: 32
            height: 32
            radius: 16
            x: 9
            y: -5
            color: card.color
        }

        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
    }

    Text {
        visible: card.badge.length > 0
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 14
        anchors.rightMargin: 56
        text: card.badge
        color: Theme.accent
        font.family: Theme.font
        font.pixelSize: 10
        font.weight: Font.DemiBold
    }

    Repeater {
        model: [[0.12, 0.22], [0.76, 0.74], [0.56, 0.18]]
        Rectangle {
            required property var modelData
            width: 3
            height: 3
            radius: 1.5
            x: card.width * modelData[0]
            y: card.height * modelData[1]
            color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b,
                           cardHover.hovered && card.interactive ? 0.38 : 0.22)
            Behavior on color { ColorAnimation { duration: Theme.motionFast } }
        }
    }

    ColumnLayout {
        id: body
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        Text {
            visible: card.title.length > 0
            text: card.title
            color: Theme.text
            font.family: Theme.font
            font.pixelSize: 16
            font.bold: true
            Layout.fillWidth: true
        }

        Text {
            visible: card.description.length > 0
            text: card.description
            color: Theme.muted
            font.family: Theme.font
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }

    HoverHandler {
        id: cardHover
        enabled: card.interactive
    }

    TapHandler {
        id: cardTap
        enabled: card.interactive
        onTapped: card.activated()
    }

    Component.onCompleted: revealProgress = 1

    Behavior on revealProgress {
        NumberAnimation { duration: Theme.motionSlow; easing.type: Easing.OutCubic }
    }
    Behavior on color { ColorAnimation { duration: Theme.motionFast } }
    Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
    Behavior on scale { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
}
