import QtQuick
import QtQuick.Layouts
import "../components"
import "../../plugins"

ColumnLayout {
    id: page
    required property var shell
    spacing: 16
    PageTitle {
        shell: page.shell
        title: "Plugins"
    }
    ExtensionSettings {
        Layout.fillWidth: true
        shell: page.shell
    }
}
