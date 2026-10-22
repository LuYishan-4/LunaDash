import QtQuick
import Quickshell

PanelWindow {
    id: panel
    required property var shell
    required property string extensionTarget
    property var extensionContext: ({})
    default property alias pluginData: slot.builtinData
    readonly property alias replacementReady: slot.replacementReady
    data: ExtensionSlot {
        id: slot
        anchors.fill: parent
        shell: panel.shell
        target: panel.extensionTarget
        context: panel.extensionContext
    }
}
