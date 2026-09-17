import QtQuick
import QtQuick.Layouts
import "../style"

ColumnLayout {
    id: picker
    property color currentColor: Theme.accent
    property real hue: currentColor.hsvHue < 0 ? 0 : currentColor.hsvHue
    property real saturation: currentColor.hsvSaturation
    property real value: currentColor.hsvValue
    property var pins: []
    property bool internalChange: false
    readonly property color selectedColor: Qt.hsva(Math.max(0, hue), saturation, value, 1)
    signal colorCommitted(string color)
    signal pinRequested(string color)
    spacing: 10

    function hexColor(color) {
        const channel = value => Math.round(Math.max(0, Math.min(1, value)) * 255).toString(16).padStart(2, "0")
        return ("#" + channel(color.r) + channel(color.g) + channel(color.b)).toLowerCase()
    }

    function syncFromCurrent() {
        if (internalChange)
            return
        hue = currentColor.hsvHue < 0 ? 0 : currentColor.hsvHue
        saturation = currentColor.hsvSaturation
        value = currentColor.hsvValue
        spectrum.requestPaint()
    }

    onCurrentColorChanged: syncFromCurrent()
    onHueChanged: spectrum.requestPaint()

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 150
        radius: 14
        clip: true
        border.width: 1
        border.color: Theme.border

        Canvas {
            id: spectrum
            anchors.fill: parent
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                ctx.fillStyle = Qt.hsva(picker.hue, 1, 1, 1)
                ctx.fillRect(0, 0, width, height)
                let white = ctx.createLinearGradient(0, 0, width, 0)
                white.addColorStop(0, "#ffffffff")
                white.addColorStop(1, "#00ffffff")
                ctx.fillStyle = white
                ctx.fillRect(0, 0, width, height)
                let dark = ctx.createLinearGradient(0, 0, 0, height)
                dark.addColorStop(0, "#00000000")
                dark.addColorStop(1, "#ff000000")
                ctx.fillStyle = dark
                ctx.fillRect(0, 0, width, height)
            }
        }

        Rectangle {
            x: Math.max(0, Math.min(parent.width - width, picker.saturation * parent.width - width / 2))
            y: Math.max(0, Math.min(parent.height - height, (1 - picker.value) * parent.height - height / 2))
            width: 16
            height: 16
            radius: 8
            color: "transparent"
            border.width: 2
            border.color: Theme.focusRing
            Rectangle { anchors.centerIn: parent; width: 5; height: 5; radius: 3; color: picker.selectedColor }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.CrossCursor
            function sample(mouse) {
                picker.saturation = Math.max(0, Math.min(1, mouse.x / width))
                picker.value = Math.max(0, Math.min(1, 1 - mouse.y / height))
            }
            onPressed: mouse => sample(mouse)
            onPositionChanged: mouse => { if (pressed) sample(mouse) }
            onReleased: picker.colorCommitted(picker.hexColor(picker.selectedColor))
        }
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 20
        radius: 10
        clip: true
        Canvas {
            id: hueStrip
            anchors.fill: parent
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                const gradient = ctx.createLinearGradient(0, 0, width, 0)
                gradient.addColorStop(0.00, "#ff0000")
                gradient.addColorStop(0.17, "#ffff00")
                gradient.addColorStop(0.33, "#00ff00")
                gradient.addColorStop(0.50, "#00ffff")
                gradient.addColorStop(0.67, "#0000ff")
                gradient.addColorStop(0.83, "#ff00ff")
                gradient.addColorStop(1.00, "#ff0000")
                ctx.fillStyle = gradient
                ctx.fillRect(0, 0, width, height)
            }
        }
        Rectangle {
            x: Math.max(0, Math.min(parent.width - width, picker.hue * parent.width - width / 2))
            anchors.verticalCenter: parent.verticalCenter
            width: 8
            height: parent.height + 4
            radius: 4
            color: "transparent"
            border.width: 2
            border.color: Theme.focusRing
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            function sample(mouse) { picker.hue = Math.max(0, Math.min(1, mouse.x / width)) }
            onPressed: mouse => sample(mouse)
            onPositionChanged: mouse => { if (pressed) sample(mouse) }
            onReleased: picker.colorCommitted(picker.hexColor(picker.selectedColor))
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        Rectangle {
            width: 34
            height: 34
            radius: 10
            color: picker.selectedColor
            border.width: 1
            border.color: Theme.border
        }
        Text {
            Layout.fillWidth: true
            text: picker.hexColor(picker.selectedColor)
            color: Theme.text
            font.family: Theme.font
            font.pixelSize: 12
        }
        ShellButton {
            text: "＋ " + qsTr("Pin")
            onClicked: picker.pinRequested(picker.hexColor(picker.selectedColor))
        }
    }

    Flow {
        Layout.fillWidth: true
        spacing: 8
        Repeater {
            model: picker.pins
            Rectangle {
                required property var modelData
                width: 34
                height: 34
                radius: 10
                color: String(modelData)
                border.width: 1
                border.color: Theme.border
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: picker.colorCommitted(String(modelData))
                }
            }
        }
    }
}
