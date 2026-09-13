import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import "../components"
import "../../components"
import "../../configuration"
import "../../effects"
ColumnLayout {
    id: page
    required property var shell
    spacing: 20
    PageTitle { shell: page.shell; title: "Appearance" }
    FileDialog { id: picker; title: shell.tr("Choose wallpaper"); nameFilters: ["Images (*.png *.jpg *.jpeg *.webp)"]; onAccepted: shell.command("wallpaper-image", selectedFile.toString()) }
    RowLayout {
        ShellButton { text: shell.tr("Choose wallpaper"); onClicked: picker.open() }
        ShellButton { text: shell.tr("Florist"); onClicked: shell.command("wallpaper-default", "") }
        ShellButton { text: shell.tr("Dusk"); onClicked: shell.command("wallpaper", 0) }
        ShellButton { text: shell.tr("Forest"); onClicked: shell.command("wallpaper", 1) }
    }
    AppearanceControls { shell: page.shell; Layout.fillWidth: true }
    EffectsControls { shell: page.shell; Layout.fillWidth: true }
}
