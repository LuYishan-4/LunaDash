import QtQuick
import "../style"

// The compositor supplies the actual scene backdrop below this layer surface.
// Do not use ShaderEffectSource on the panel itself: that would blur its text.
Rectangle {
    id: surface
    required property var shell
    property real tintOpacity: 0.62
    readonly property var appearance: shell.state.appearance || ({})
    readonly property bool glassAvailable: appearance.blur === true
        && !appearance.eyeCare && shell.state.blurReady === true
        && shell.state.blurFailed !== true
    radius: 24
    color: glassAvailable
        ? Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, tintOpacity)
        : Theme.base
    border.width: 1
    border.color: Theme.hairline
    Behavior on color { ColorAnimation { duration: Theme.motionFast } }
}
