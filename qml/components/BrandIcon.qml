import QtQuick

Item {
    id: root
    required property var shell
    property alias source: image.source
    property alias smooth: image.smooth

    implicitWidth: 28
    implicitHeight: 24

    Image {
        id: image
        anchors.fill: parent
        source: root.shell.iconSource
        sourceClipRect: Qt.rect(610, 520, 440, 330)
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        mipmap: true
        smooth: true
    }
}
