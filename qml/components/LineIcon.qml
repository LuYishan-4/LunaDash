import QtQuick
import QtQuick.Shapes
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
        sound: "M3 9 H7 L12 5 V19 L7 15 H3 Z M16 8 Q21 12 16 16 M19 5 Q26 12 19 19",
        network: "M3 8 Q12 1 21 8 M6 12 Q12 7 18 12 M9 16 Q12 13 15 16 M12 20 H12.1",
        bluetooth: "M8 7 L17 16 L12 21 V3 L17 8 L8 17",
        power: "M4 6 H19 V18 H4 Z M22 10 V14 M12 8 L9 13 H14 L11 17",
        applications: "M4 4 H20 V20 H4 Z M4 9 H20 M8 6.5 H8.1 M12 13 L9 17 M13 17 H17",
        privacy: "M12 3 L20 6 V12 Q20 18 12 22 Q4 18 4 12 V6 Z M8 12 L11 15 L16 9",
        system: "M12 3 A4 4 0 1 1 12 11 A4 4 0 1 1 12 3 M4 21 V18 Q4 14 12 14 Q20 14 20 18 V21",
        devices: "M5 3 H19 V21 H5 Z M5 15 H19 M9 18 H9.1 M15 18 H15.1",
        about: "M12 3 A9 9 0 1 1 12 21 A9 9 0 1 1 12 3 M12 10 V17 M12 7 H12.1",
        search: "M10 3 A7 7 0 1 1 10 17 A7 7 0 1 1 10 3 M16 16 L22 22",
        close: "M6 6 L18 18 M18 6 L6 18",
        files: "M3 7 H10 L12 9 H21 V20 H3 Z",
        settings: "M12 3 V6 M12 18 V21 M3 12 H6 M18 12 H21 M5.6 5.6 L7.8 7.8 M16.2 16.2 L18.4 18.4 M18.4 5.6 L16.2 7.8 M7.8 16.2 L5.6 18.4 M12 8 A4 4 0 1 1 12 16 A4 4 0 1 1 12 8",
        terminal: "M3 5 H21 V19 H3 Z M7 9 L10 12 L7 15 M12 15 H17",
        monitor: "M3 19 H21 M5 16 L9 11 L12 14 L17 7 L20 10"
    })
    Shape {
        anchors.centerIn: parent; width: 24; height: 24; scale: Math.min(root.width, root.height) / 24
        ShapePath { strokeColor: root.ink; strokeWidth: 1.6; fillColor: "transparent"; capStyle: ShapePath.RoundCap; joinStyle: ShapePath.RoundJoin; PathSvg { path: root.paths[root.name] || root.paths.general } }
    }
}
