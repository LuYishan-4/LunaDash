import QtQuick
import QtQuick.Layouts
import "../components"
import "../../components"
import "../../configuration"
import "../../effects"
import "../../wallpaper"
import "../../launcher"
import "../../style"

ColumnLayout {
    id: page
    required property var shell
    readonly property alias firstFocusItem: panelSettings.firstFocusItem
    spacing: 16

    PageTitle { shell: page.shell; title: "Appearance" }
    PanelSettings { id: panelSettings; shell: page.shell }
    WallpaperLibrary { shell: page.shell; Layout.fillWidth: true }
    ThemeControls { shell: page.shell; Layout.fillWidth: true }
    WidgetControls { shell: page.shell; Layout.fillWidth: true }
    AppearanceControls { shell: page.shell; Layout.fillWidth: true }
    EffectsControls { shell: page.shell; Layout.fillWidth: true }
    OrbitSettings { shell: page.shell; Layout.fillWidth: true }
}
