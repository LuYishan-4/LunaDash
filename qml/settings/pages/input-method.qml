import QtQuick
import QtQuick.Layouts
import "../components"
import "../components" as SettingsComponents
import "../../components"
import "../../style"

ColumnLayout {
    id: page
    required property var shell
    spacing: 16
    readonly property var inputTools: (shell.state.systemTools || []).filter(tool => tool.category === "input")
    readonly property var imeTool: inputTools.find(tool => tool.id === "ime") || ({available:false, package:"fcitx5-configtool"})

    PageTitle { shell: page.shell; title: "Input method" }

    SettingsComponents.SettingsCard {
        title: shell.tr("Fcitx 5")
        description: shell.tr("Configure input methods, addons, hotkeys, candidate behavior, and per-application options for LunaDash sessions.")
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                Text { text: page.imeTool.available ? shell.tr("Fcitx configuration tool is available") : shell.tr("Fcitx configuration tool is not installed"); color: Theme.text; font.family: Theme.font; font.pixelSize: 14 }
                HelpText { shell: page.shell; message: page.imeTool.available ? "Open the Fcitx 5 configuration utility to manage input methods and addons." : "Install fcitx5-configtool. LunaDash will use it automatically when available." }
            }
            ShellButton { text: shell.tr("Configure Fcitx"); active: true; enabled: page.imeTool.available; onClicked: shell.command("system-tool", "ime") }
        }
    }

    SettingsComponents.SettingsCard {
        title: shell.tr("Session integration")
        description: shell.tr("LunaDash starts Fcitx 5 for standalone sessions and exports compatibility variables for Qt, GTK, SDL, XWayland, and native Wayland clients.")
        GridLayout {
            Layout.fillWidth: true; columns: 2; columnSpacing: 16; rowSpacing: 8
            Text { text: "Qt"; color: Theme.muted; font.family: Theme.font }
            Text { Layout.minimumWidth: 0; Layout.fillWidth: true; text: "QT_IM_MODULE=fcitx · QT_IM_MODULES=wayland;fcitx;ibus"; color: Theme.text; font.family: Theme.font; font.pixelSize: 11; elide: Text.ElideMiddle }
            Text { text: "GTK"; color: Theme.muted; font.family: Theme.font }
            Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0; Layout.fillWidth: true; text: "GTK_IM_MODULE=fcitx"; color: Theme.text; font.family: Theme.font; font.pixelSize: 11 }
            Text { text: "X11"; color: Theme.muted; font.family: Theme.font }
            Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0; Layout.fillWidth: true; text: "XMODIFIERS=@im=fcitx"; color: Theme.text; font.family: Theme.font; font.pixelSize: 11 }
            Text { text: "SDL"; color: Theme.muted; font.family: Theme.font }
            Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0; Layout.fillWidth: true; text: "SDL_IM_MODULE=fcitx"; color: Theme.text; font.family: Theme.font; font.pixelSize: 11 }
        }
    }

    SettingsComponents.SettingsCard {
        title: shell.tr("Input test")
        description: shell.tr("Verify preedit text, candidate selection, commit, deletion, cursor movement, and focus changes here.")
        SoftField { Layout.fillWidth: true; placeholderText: shell.tr("Type here to test Fcitx 5"); Accessible.name: placeholderText }
    }
}
