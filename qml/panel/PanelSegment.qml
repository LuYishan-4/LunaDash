import QtQuick
import "../components"
Segment {
    required property var moduleHost
    implicitHeight: Math.max(16, moduleHost.height - 8)
    ink: moduleHost.moduleForeground
    accentColor: moduleHost.moduleAccent
    textSize: moduleHost.moduleFontSize
}
