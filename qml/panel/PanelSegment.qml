import QtQuick
import "../components"
import "../style"

Segment {
    required property var moduleHost
    implicitHeight: Math.max(16, moduleHost.height - 8)
    ink: moduleHost.moduleForeground
    accentColor: moduleHost.moduleAccent
    textSize: moduleHost.moduleFontSize
    fill: Qt.rgba(moduleHost.moduleAccent.r,
                  moduleHost.moduleAccent.g,
                  moduleHost.moduleAccent.b,
                  0.16)
    border.width: activeFocus ? 1.5 : 1
    border.color: activeFocus
        ? moduleHost.moduleAccent
        : Qt.rgba(moduleHost.moduleAccent.r,
                  moduleHost.moduleAccent.g,
                  moduleHost.moduleAccent.b,
                  0.34)
}
