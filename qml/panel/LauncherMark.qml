import QtQuick
import QtQuick.Window
import "../components"
import "../style"

Item {
    id: mark
    property string source: ""
    property color accent: Theme.accent
    implicitWidth: 36
    implicitHeight: 36

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.rgba(mark.accent.r, mark.accent.g, mark.accent.b, 0.22) }
            GradientStop { position: 1; color: Qt.rgba(mark.accent.r, mark.accent.g, mark.accent.b, 0.04) }
        }
        border.width: 1
        border.color: Qt.rgba(mark.accent.r, mark.accent.g, mark.accent.b, 0.48)
    }

    Image {
        id: customImage
        anchors.fill: parent
        anchors.margins: Math.max(2, mark.width * 0.08)
        source: mark.source
        sourceSize.width: Math.max(1, Math.round(width * Screen.devicePixelRatio))
        sourceSize.height: Math.max(1, Math.round(height * Screen.devicePixelRatio))
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        smooth: true
        visible: status === Image.Ready
    }
    LunaDashLogo {
        anchors.fill: parent
        anchors.margins: 1
        visible: !customImage.visible
        animated: false
        primaryColor: mark.accent
        secondaryColor: Qt.lighter(mark.accent, 1.22)
        inkColor: Theme.text
    }
    Rectangle {
        visible: !customImage.visible
        width: 3
        height: 3
        radius: 1.5
        x: mark.width * 0.75
        y: mark.height * 0.74
        color: Theme.text
        opacity: 0.8
    }
}
