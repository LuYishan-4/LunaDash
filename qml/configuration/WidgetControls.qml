import QtQuick
import QtQuick.Layouts
import "../components"
import "../settings/components"
import "../style"

SettingsCard {
    id: controls
    required property var shell
    readonly property var preferences: shell.state.appearance || ({})
    title: shell.tr("Desktop widgets")
    description: shell.tr("Clock and audio rings stay on the wallpaper. Audio rings monitor playback, never the microphone.")
    SettingsTargetEditor {
        Layout.fillWidth: true
        shell: controls.shell
        targetId: "builtin:desktop-widgets"
    }
    Text {
        Layout.fillWidth: true
        text: shell.tr("Weather uses coordinates you enter and sends them to Open-Meteo every 15 minutes when enabled.")
        color: Theme.muted
        font.family: Theme.font
        wrapMode: Text.Wrap
    }
    SoftField {
        id: location
        Layout.fillWidth: true
        text: controls.preferences.weatherLocation || ""
        placeholderText: shell.tr("Location label")
    }
    GridLayout {
        Layout.fillWidth: true
        columns: controls.width > 550 ? 2 : 1
        SoftField {
            id: latitude
            Layout.fillWidth: true
            text: String(controls.preferences.weatherLatitude ?? 0)
            placeholderText: shell.tr("Latitude")
            validator: DoubleValidator {
                bottom: -90
                top: 90
                locale: "C"
            }
        }
        SoftField {
            id: longitude
            Layout.fillWidth: true
            text: String(controls.preferences.weatherLongitude ?? 0)
            placeholderText: shell.tr("Longitude")
            validator: DoubleValidator {
                bottom: -180
                top: 180
                locale: "C"
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        ShellButton {
            Layout.fillWidth: true
            text: controls.preferences.weatherEnabled ? shell.tr("Disable weather") : shell.tr("Enable weather")
            active: controls.preferences.weatherEnabled ?? false
            enabled: latitude.acceptableInput && longitude.acceptableInput
            onClicked: shell.setAppearance({
                weatherEnabled: !active,
                weatherLocation: location.text,
                weatherLatitude: Number(latitude.text),
                weatherLongitude: Number(longitude.text)
            })
        }
        ShellButton {
            text: shell.tr("Save location")
            enabled: latitude.acceptableInput && longitude.acceptableInput
            onClicked: shell.setAppearance({
                weatherLocation: location.text,
                weatherLatitude: Number(latitude.text),
                weatherLongitude: Number(longitude.text)
            })
        }
    }
    Text {
        Layout.fillWidth: true
        visible: Boolean((shell.state.weather || {}).error)
        text: shell.tr((shell.state.weather || {}).error || "")
        color: Theme.warning
        font.family: Theme.font
        wrapMode: Text.Wrap
    }
}
