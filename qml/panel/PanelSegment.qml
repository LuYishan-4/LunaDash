import QtQuick
import "../components"
import "../style"

Segment {
    required property var moduleHost
    implicitHeight: Math.max(16, moduleHost.height - 8)
    ink: moduleHost.moduleForeground
    accentColor: moduleHost.moduleAccent
    textSize: moduleHost.moduleFontSize
    fill: Qt.rgba(Theme.secondaryAccent.r,
                  Theme.secondaryAccent.g,
                  Theme.secondaryAccent.b,
                  0.34)
    border.width: activeFocus ? 1.5 : 1
    border.color: activeFocus
        ? Theme.moon
        : Qt.rgba(Theme.starlight.r,
                  Theme.starlight.g,
                  Theme.starlight.b,
                  0.30)
}
