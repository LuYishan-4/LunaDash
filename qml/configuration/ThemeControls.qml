import QtQuick
import QtQuick.Layouts
import "../components"
import "../settings/components"
import "../style"

SettingsCard {
    id: controls
    required property var shell
    readonly property var appearance: shell.state.appearance || ({})
    title: shell.tr("Colors and comfort")
    description: shell.tr("Wallpaper colors, light and dark surfaces, and a warmer desktop.")
    RowLayout {
        Layout.fillWidth: true
        Repeater {
            model: [
                {
                    id: "dark",
                    label: "Dark"
                },
                {
                    id: "light",
                    label: "Light"
                },
                {
                    id: "auto",
                    label: "Automatic"
                }
            ]
            ShellButton {
                required property var modelData
                Layout.fillWidth: true
                text: shell.tr(modelData.label)
                active: controls.appearance.themeMode === modelData.id
                onClicked: shell.setAppearance({
                    themeMode: modelData.id
                })
            }
        }
    }
    Text {
        Layout.fillWidth: true
        text: shell.tr("Automatic uses light colors from 07:00 to 19:00, in your local time.")
        color: Theme.muted
        font.family: Theme.font
        wrapMode: Text.Wrap
    }
    GridLayout {
        Layout.fillWidth: true
        columns: controls.width > 650 ? 2 : 1
        Repeater {
            model: [
                {
                    key: "wallpaperColors",
                    label: "Colors from wallpaper"
                },
                {
                    key: "eyeCare",
                    label: "Eye care"
                },
                {
                    key: "dockEnabled",
                    label: "Show dock"
                },
                {
                    key: "dockAutoHide",
                    label: "Automatically hide dock"
                }
            ]
            ShellButton {
                required property var modelData
                Layout.fillWidth: true
                text: shell.tr(modelData.label)
                active: Boolean(controls.appearance[modelData.key])
                onClicked: shell.setAppearance({
                    [modelData.key]: !active
                })
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        Text {
            Layout.fillWidth: true
            text: shell.tr("Color temperature")
            color: Theme.text
            font.family: Theme.font
            wrapMode: Text.Wrap
        }
        SoftSlider {
            Layout.fillWidth: true
            from: 2500
            to: 6500
            stepSize: 100
            value: controls.appearance.eyeCareTemperature || 4500
            onMoved: if (!pressed)
                shell.setAppearance({
                    eyeCareTemperature: Math.round(value)
                })
            onPressedChanged: if (!pressed)
                shell.setAppearance({
                    eyeCareTemperature: Math.round(value)
                })
        }
        Text {
            text: String(controls.appearance.eyeCareTemperature || 4500) + " K"
            color: Theme.accent
            font.family: Theme.font
        }
    }
    Repeater {
        model: ((shell.state.display || {}).nightLight || []).filter(output => output.error)
        Text {
            required property var modelData
            Layout.fillWidth: true
            text: modelData.output + ": " + shell.tr(modelData.error)
            color: Theme.warning
            font.family: Theme.font
            wrapMode: Text.Wrap
        }
    }
    ShellButton {
        Layout.fillWidth: true
        text: shell.tr("Synchronize application themes")
        active: controls.appearance.syncApplicationThemes ?? false
        onClicked: shell.setAppearance({
            syncApplicationThemes: !active
        })
    }
    Text {
        Layout.fillWidth: true
        text: shell.tr("Opt in to GTK 3/4 and Kitty theme files. Existing CSS and Kitty settings receive a backed-up include. Browsers follow the desktop portal; Kitty reads its colors on reload.")
        color: Theme.muted
        font.family: Theme.font
        wrapMode: Text.Wrap
    }
    Text {
        Layout.fillWidth: true
        visible: Boolean((shell.state.applicationTheme || {}).error)
        text: (shell.state.applicationTheme || {}).error || ""
        color: Theme.warning
        font.family: Theme.font
        wrapMode: Text.Wrap
    }
    ShellButton {
        Layout.fillWidth: true
        text: shell.tr("Synchronize Fcitx 5 theme")
        active: controls.appearance.syncFcitxTheme ?? false
        onClicked: shell.setAppearance({
            syncFcitxTheme: !active
        })
    }
    Text {
        Layout.fillWidth: true
        text: shell.tr("Opt in to LunaDash Mellow, adapted from NyxMellow. Your Fcitx appearance settings are backed up before selecting the theme.")
        color: Theme.muted
        font.family: Theme.font
        wrapMode: Text.Wrap
    }
    RowLayout {
        Layout.fillWidth: true
        SoftField {
            id: presetName
            Layout.fillWidth: true
            placeholderText: shell.tr("Preset name")
        }
        ShellButton {
            text: shell.tr("Save preset")
            enabled: presetName.text.trim().length > 0
            onClicked: shell.command("appearance-preset-save", presetName.text.trim())
        }
    }
    RowLayout {
        Layout.fillWidth: true
        StyledComboBox {
            id: presets
            Layout.fillWidth: true
            model: (shell.state.appearancePresets || []).map(preset => preset.name)
        }
        ShellButton {
            text: shell.tr("Apply preset")
            enabled: presets.count > 0
            onClicked: shell.command("appearance-preset-apply", presets.currentText)
        }
        ShellButton {
            iconName: "close"
            Accessible.name: shell.tr("Delete preset")
            enabled: presets.count > 0
            onClicked: shell.command("appearance-preset-delete", presets.currentText)
        }
    }
}
