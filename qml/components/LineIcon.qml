import QtQuick
import QtQuick.Shapes
import QtQuick.Window
import "../style"

Item {
    id: root
    property string name: "general"
    property color ink: Theme.muted
    implicitWidth: 22; implicitHeight: 22
    readonly property var paths: ({
        general: "M4 6 H20 M4 12 H20 M4 18 H20 M8 3 V9 M16 9 V15 M10 15 V21",
        appearance: "M12 3 C6 3 3 7 3 12 C3 18 8 21 12 21 H15 C18 21 18 17 15 17 C12 17 14 14 17 14 C23 14 22 3 12 3 M7 10 H7.1 M10 6 H10.1 M16 7 H16.1",
        windows: "M4 4 H20 V20 H4 Z M12 4 V20 M12 12 H20",
        modules: "M4 4 H10 V10 H4 Z M14 4 H20 V10 H14 Z M4 14 H10 V20 H4 Z M14 14 H20 V20 H14 Z",
        display: "M3 4 H21 V17 H3 Z M12 17 V21 M8 21 H16",
        input: "M3 6 H21 V18 H3 Z M6 10 H7 M10 10 H11 M14 10 H15 M18 10 H18.1 M7 14 H17",
        "input-method": "M4 5 H20 V19 H4 Z M7 9 H17 M7 13 H13 M16 13 H17 M7 16 H10 M14 16 H17",
        sound: "M3 9 H7 L12 5 V19 L7 15 H3 Z M16 8 Q21 12 16 16 M19 5 Q26 12 19 19",
        network: "M3 8 Q12 1 21 8 M6 12 Q12 7 18 12 M9 16 Q12 13 15 16 M12 20 H12.1",
        weather: "M8.5 18 H18 A4 4 0 0 0 18 10 A5.5 5.5 0 0 0 7.6 8.2 A4.2 4.2 0 0 0 8.5 18 M5 5 L3.5 3.5 M5 11 H2 M9 3 V1",
        cloud: "M7 18 H18 A4 4 0 0 0 18 10 A5.5 5.5 0 0 0 7.6 8.2 A4.2 4.2 0 0 0 7 18",
        bluetooth: "M8 7 L17 16 L12 21 V3 L17 8 L8 17",
        power: "M4 6 H19 V18 H4 Z M22 10 V14 M12 8 L9 13 H14 L11 17",
        applications: "M4 4 H20 V20 H4 Z M4 9 H20 M8 6.5 H8.1 M12 13 L9 17 M13 17 H17",
        privacy: "M12 3 L20 6 V12 Q20 18 12 22 Q4 18 4 12 V6 Z M8 12 L11 15 L16 9",
        system: "M12 3 A4 4 0 1 1 12 11 A4 4 0 1 1 12 3 M4 21 V18 Q4 14 12 14 Q20 14 20 18 V21",
        devices: "M5 3 H19 V21 H5 Z M5 15 H19 M9 18 H9.1 M15 18 H15.1",
        usb: "M12 3 V16 M12 3 L9 6 M12 3 L15 6 M12 9 L7 9 V13 M12 11 H17 V8 M17 8 L15 10 M17 8 L19 10 M7 13 A2 2 0 1 0 7 17 A2 2 0 1 0 7 13 M12 16 A2 2 0 1 1 12 20 A2 2 0 1 1 12 16",
        about: "M12 3 A9 9 0 1 1 12 21 A9 9 0 1 1 12 3 M12 10 V17 M12 7 H12.1",
        search: "M10 3 A7 7 0 1 1 10 17 A7 7 0 1 1 10 3 M16 16 L22 22",
        close: "M6 6 L18 18 M18 6 L6 18",
        files: "M3 7 H10 L12 9 H21 V20 H3 Z",
        settings: "M12 3 V6 M12 18 V21 M3 12 H6 M18 12 H21 M5.6 5.6 L7.8 7.8 M16.2 16.2 L18.4 18.4 M18.4 5.6 L16.2 7.8 M7.8 16.2 L5.6 18.4 M12 8 A4 4 0 1 1 12 16 A4 4 0 1 1 12 8",
        terminal: "M3 5 H21 V19 H3 Z M7 9 L10 12 L7 15 M12 15 H17",
        monitor: "M3 19 H21 M5 16 L9 11 L12 14 L17 7 L20 10",
        shortcuts: "M5 5 H19 V19 H5 Z M8 9 H9 M12 9 H13 M16 9 H17 M8 13 H9 M12 13 H17 M8 16 H16",
        update: "M12 4 A8 8 0 1 1 5 8 M5 4 V8 H9 M12 8 V13 L16 15",
        github: "M15 22v-4a4.8 4.8 0 0 0-1-3.5c3 0 6-2 6-5.5c.08-1.25-.27-2.48-1-3.5c.28-1.15.28-2.35 0-3.5c0 0-1 0-3 1.5c-2.64-.5-5.36-.5-8 0C6 2 5 2 5 2c-.3 1.15-.3 2.35 0 3.5A5.4 5.4 0 0 0 4 9c0 3.5 3 5.5 6 5.5c-.39.49-.68 1.05-.85 1.65S8.93 17.38 9 18v4 M9 18c-4.51 2-5-2-7-2",
        discord: "M7 7 C10 5 14 5 17 7 C19 10 20 14 19 17 C17 19 16 19 14 18 L13 16 C15 16 16 15 17 14 C14 16 10 16 7 14 C8 15 9 16 11 16 L10 18 C8 19 7 19 5 17 C4 14 5 10 7 7 M9 11 H9.1 M15 11 H15.1",
        moon: "M15 3 C9 4 6 9 8 15 C10 20 16 22 21 18 C16 18 12 14 12 9 C12 6 13 4 15 3",
        chevronDown: "M6 9 L12 15 L18 9",
        copy: "M8 8 H19 V19 H8 Z M5 16 V5 H16 V6",
        paste: "M6 5 H18 V21 H6 Z M9 5 V3 H15 V5 Z M9 11 H15 M9 15 H14",
        apps: "M4 4 H10 V10 H4 Z M14 4 H20 V10 H14 Z M4 14 H10 V20 H4 Z M14 14 H20 V20 H14 Z"
    })
    readonly property string pathData: root.paths[root.name] || root.paths.general

    function hexColor(value) {
        const part = component => Math.max(0, Math.min(255, Math.round(component * 255))).toString(16).padStart(2, "0")
        return "#" + part(value.r) + part(value.g) + part(value.b)
    }

    readonly property string svgData: "data:image/svg+xml;utf8," + encodeURIComponent(
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' width='24' height='24'>" +
        "<path d='" + root.pathData + "' fill='none' stroke='" + root.hexColor(root.ink) + "' stroke-width='1.6' stroke-linecap='round' stroke-linejoin='round'/></svg>")

    Image {
        id: raster
        anchors.fill: parent
        source: root.svgData
        sourceSize.width: Math.max(1, Math.round(root.width * Screen.devicePixelRatio))
        sourceSize.height: Math.max(1, Math.round(root.height * Screen.devicePixelRatio))
        fillMode: Image.PreserveAspectFit
        smooth: true
    }

    Shape {
        anchors.centerIn: parent; width: 24; height: 24; scale: Math.min(root.width, root.height) / 24
        visible: raster.status === Image.Error
        ShapePath { strokeColor: root.ink; strokeWidth: 1.6; fillColor: "transparent"; capStyle: ShapePath.RoundCap; joinStyle: ShapePath.RoundJoin; PathSvg { path: root.pathData } }
    }
}
