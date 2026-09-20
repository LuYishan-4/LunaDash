import QtQuick

Loader {
    id: host
    required property var shell
    active: !shell.stopping
    sourceComponent: ExtensionSlot {
        shell: host.shell
        target: "desktop-widgets"
    }
}
