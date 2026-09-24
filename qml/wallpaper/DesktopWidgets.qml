import QtQuick
import QtQuick.Layouts
import Quickshell.Io
import "../components"
import "../style"

Item {
    id: widgets
    required property var shell
    property var settings: ({})
    readonly property var weather: shell.state.weather || ({})
    property string time: ""
    property string date: ""
    property var bands: []
    readonly property bool spectrumEnabled: (settings.visualizerEnabled ?? false) && !shell.stopping
    readonly property color ink: Theme.dark ? Theme.text : Theme.accent

    Timer {
        interval: 1000
        repeat: true
        running: widgets.visible && (widgets.settings.clockEnabled ?? true)
        triggeredOnStart: true
        onTriggered: {
            widgets.time = Qt.formatDateTime(new Date(), Theme.clock24Hour ? "HH:mm" : "h:mm AP");
            widgets.date = Qt.formatDateTime(new Date(), "dddd, MMMM d");
        }
    }
    ColumnLayout {
        visible: widgets.settings.clockEnabled ?? true
        x: Math.min(Math.max(12, widgets.width * (widgets.settings.clockX ?? 3) / 100), Math.max(12, widgets.width - width - 12))
        y: Theme.panelTopInset + 24 + Math.max(0, (widgets.height - height - Theme.panelTopInset - 48) * (widgets.settings.clockY ?? 0) / 100)
        width: Math.min(520, Math.max(1, widgets.width - 48))
        spacing: 3
        Text {
            Layout.fillWidth: true
            text: widgets.time
            color: widgets.ink
            font.family: Theme.font
            font.pixelSize: Math.min(100, widgets.width * 0.08)
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
        Text {
            Layout.fillWidth: true
            text: widgets.date
            color: widgets.ink
            font.family: Theme.font
            font.pixelSize: 18
            elide: Text.ElideRight
        }
        RowLayout {
            Layout.fillWidth: true
            visible: widgets.weather.enabled ?? false
            LineIcon {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                name: "weather"
                ink: widgets.ink
            }
            Text {
                Layout.fillWidth: true
                text: widgets.weather.available ? widgets.weather.temperature + "°C · " + shell.tr(widgets.weather.condition) : shell.tr("Weather data unavailable")
                color: widgets.ink
                font.family: Theme.font
                font.pixelSize: 20
                elide: Text.ElideRight
            }
        }
        Text {
            Layout.fillWidth: true
            visible: widgets.weather.enabled ?? false
            text: (widgets.weather.location || "") + " · Open-Meteo"
            color: widgets.ink
            font.family: Theme.font
            font.pixelSize: 11
            elide: Text.ElideRight
        }
    }

    Process {
        id: audio
        command: [widgets.shell.shellToolExecutable, "audio-spectrum"]
        running: widgets.spectrumEnabled
        stdout: SplitParser {
            onRead: line => {
                try {
                    const frame = JSON.parse(line);
                    widgets.bands = frame.available ? frame.bands || [] : [];
                } catch (error) {
                    widgets.bands = [];
                }
            }
        }
    }
    Item {
        visible: widgets.spectrumEnabled
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.42, parent.height * 0.55, 480)
        height: width
        Repeater {
            model: 64
            Rectangle {
                required property int index
                readonly property real level: Math.max(0, Math.min(1, Number(widgets.bands[index % 32] || 0)))
                width: 3
                height: 5 + level * parent.width * 0.18
                radius: 1.5
                color: Theme.accent
                opacity: 0.55 + level * 0.4
                x: parent.width / 2 + Math.sin(index * Math.PI / 32) * parent.width * 0.30 - width / 2
                y: parent.height / 2 - Math.cos(index * Math.PI / 32) * parent.height * 0.30 - height
                transformOrigin: Item.Bottom
                rotation: index * 360 / 64
                Behavior on height {
                    NumberAnimation {
                        duration: Theme.animations ? 70 : 0
                    }
                }
            }
        }
    }
}
