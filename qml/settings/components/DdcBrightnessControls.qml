import QtQuick
import QtQuick.Layouts
import "../../components"
import "../../style"

SettingsCard {
    id: controls
    required property var shell
    readonly property var status: shell.state.ddcBrightness || ({})
    title: shell.tr("External monitor brightness")
    description: shell.tr("Adjust each monitor through DDC/CI.")

    ShellButton {
        Layout.fillWidth: true
        text: controls.status.scanning ? shell.tr("Detecting monitors…") : shell.tr("Refresh monitors")
        enabled: !controls.status.busy && !controls.status.scanning
        onClicked: controls.shell.command("ddc-refresh", "")
    }
    HelpText {
        shell: controls.shell
        visible: Boolean(controls.status.error)
        message: controls.status.error || ""
    }
    HelpText {
        shell: controls.shell
        visible: controls.status.installed && !controls.status.scanning && !(controls.status.devices || []).length
        message: "No DDC/CI monitors detected. Enable DDC/CI in the monitor menu, then refresh."
    }
    Repeater {
        // Keep delegates alive across status polling so dragging and debouncing
        // are not interrupted whenever the backend publishes a new JSON array.
        model: (controls.status.devices || []).length
        ColumnLayout {
            id: monitor
            required property int index
            readonly property var device: (controls.status.devices || [])[index] || ({})
            property int pending: -1
            property string pendingId: ""
            Layout.fillWidth: true
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    wrapMode: Text.Wrap
                    text: monitor.device.label || ""
                    color: Theme.text; font.family: Theme.font
                }
                Text {
                    text: monitor.device.available ? Math.round(slider.value) + "%" : "—"
                    color: Theme.muted; font.family: Theme.font
                }
            }
            SoftSlider {
                id: slider
                objectName: "ddcBrightnessSlider"
                Layout.fillWidth: true
                from: 0; to: 100; stepSize: 1
                enabled: Boolean(monitor.device.available) && !controls.status.scanning
                Accessible.name: monitor.device.label || ""
                onMoved: {
                    monitor.pending = Math.round(value)
                    monitor.pendingId = monitor.device.id
                    debounce.restart()
                }
                Binding {
                    target: slider; property: "value"
                    value: monitor.device.percent || 0
                    when: !slider.pressed && monitor.pending < 0 && !monitor.device.busy
                    restoreMode: Binding.RestoreNone
                }
            }
            HelpText {
                shell: controls.shell
                visible: Boolean(monitor.device.error)
                message: monitor.device.error || ""
            }
            Timer {
                id: debounce
                interval: 200
                onTriggered: {
                    if (monitor.pendingId === monitor.device.id && monitor.device.available && !controls.status.scanning)
                        controls.shell.command("ddc-brightness", JSON.stringify({id: monitor.pendingId, percent: monitor.pending}))
                    monitor.pending = -1
                }
            }
        }
    }
    HelpText {
        shell: controls.shell
        message: "Requires ddcutil, enabled DDC/CI and access to the monitor's I2C device."
    }
}
