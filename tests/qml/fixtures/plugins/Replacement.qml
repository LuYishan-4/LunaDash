import QtQuick
Rectangle {
    required property var shell
    required property var settings
    required property var context
    property bool pluginReady: settings.ready ?? true
    objectName: "testPlugin"
    color: settings.color
    implicitHeight: 70
}
