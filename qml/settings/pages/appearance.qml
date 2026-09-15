import QtQuick
import QtQuick.Layouts

import "../components"
import "../../components"
import "../../configuration"
import "../../effects"
ColumnLayout {
    id: page
    required property var shell
    spacing: 16
    PageTitle { shell: page.shell; title: "Appearance" }

    RowLayout {
        ShellButton { text: shell.tr("Choose wallpaper"); onClicked: shell.pickerOpen = true }
        ShellButton { text: shell.tr("Florist"); onClicked: shell.command("wallpaper-default", "") }
        ShellButton { text: shell.tr("Dusk"); onClicked: shell.command("wallpaper", 0) }
        ShellButton { text: shell.tr("Forest"); onClicked: shell.command("wallpaper", 1) }
    }
    AppearanceControls { shell: page.shell; Layout.fillWidth: true }
    EffectsControls { shell: page.shell; Layout.fillWidth: true }
}
