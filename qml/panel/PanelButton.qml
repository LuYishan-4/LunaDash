import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../style"

PanelSegment {
    id: button
    property string iconName: ""
    property string label: ""
    property string toolTip: ""
    signal scrolled(real delta)
    implicitWidth: label.length ? Math.max(34, caption.implicitWidth + (iconName.length ? 24 : 0) + 20) : implicitHeight
    implicitHeight: moduleHost.capsuleHeight
    fill: moduleHost.contrastShells ? moduleHost.capsuleColor(0.84, 0.94) : "transparent"
    border.width: activeFocus ? 1.5 : moduleHost.contrastShells ? 1 : 0
    border.color: activeFocus ? Theme.focusRing : Theme.hairline
    Accessible.name: toolTip || label
    Accessible.onPressAction: if (enabled) clicked()

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Math.min(9, Math.max(0, (button.width - 16) / 2))
        anchors.rightMargin: Math.min(9, Math.max(0, (button.width - 16) / 2))
        spacing: 5
        LineIcon {
            visible: button.iconName.length > 0 && button.label.length > 0
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            name: button.iconName
            ink: button.selected ? Theme.accentInk : button.moduleHost.moduleAccent
        }
        Text {
            id: caption
            visible: button.label.length > 0
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: button.label
            textFormat: Text.PlainText
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            color: button.selected ? Theme.accentInk : button.moduleHost.moduleForeground
            font.family: Theme.font
            font.pixelSize: 12
            font.weight: Font.DemiBold
        }
    }
    LineIcon {
        anchors.centerIn: parent
        width: 16
        height: 16
        visible: button.iconName.length > 0 && button.label.length === 0
        name: button.iconName
        ink: button.selected ? Theme.accentInk : button.moduleHost.moduleAccent
    }
    HoverHandler { id: hover }
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        onWheel: wheel => button.scrolled(wheel.angleDelta.y)
    }
    ToolTip.visible: hover.hovered && toolTip.length > 0
    ToolTip.delay: 450
    ToolTip.text: toolTip
}
