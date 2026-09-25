import QtQuick
import QtQuick.Window
import "../components"
import "../style"

Item {
    id: mark
    property string source: ""
    property color accent: Theme.accent
    implicitWidth: 28
    implicitHeight: 28

    Image {
        id: customImage
        anchors.fill: parent
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
        visible: !customImage.visible
        animated: false
        primaryColor: mark.accent
        secondaryColor: Qt.lighter(mark.accent, 1.22)
        inkColor: Theme.text
    }
}
