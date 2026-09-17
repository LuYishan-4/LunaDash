import QtQuick
import "../style"

Item {
    id: root
    property color accent: Theme.accent
    property real strength: 1.0
    clip: true

    // Deterministic decoration keeps every surface visually related without
    // introducing another animation system or random state.
    Repeater {
        model: [
            [0.08, 0.18, 2], [0.18, 0.72, 1], [0.31, 0.28, 1],
            [0.43, 0.84, 2], [0.57, 0.16, 1], [0.68, 0.62, 1],
            [0.79, 0.34, 2], [0.91, 0.76, 1], [0.52, 0.52, 1]
        ]
        Rectangle {
            required property var modelData
            width: modelData[2] * 2
            height: width
            radius: width / 2
            x: Math.max(4, (root.width - width - 4) * modelData[0])
            y: Math.max(4, (root.height - height - 4) * modelData[1])
            color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b,
                           0.16 * root.strength)
        }
    }

    Item {
        width: 74
        height: 74
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 18
        anchors.topMargin: 14
        opacity: 0.12 * root.strength

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: root.accent
        }
        Rectangle {
            width: parent.width
            height: parent.height
            radius: width / 2
            x: 22
            y: -8
            color: Theme.background
        }
    }

    Behavior on opacity {
        NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic }
    }
}
