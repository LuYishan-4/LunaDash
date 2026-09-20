import QtQuick
import "../style"

Item {
    id: mascot
    property bool playing: false
    property real sway: 0
    implicitWidth: 190
    implicitHeight: 160
    Image {
        anchors.centerIn: parent
        width: parent.width
        height: parent.height
        fillMode: Image.PreserveAspectFit
        rotation: mascot.sway * 7 - 3.5
        source: "data:image/svg+xml;utf8," + encodeURIComponent("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 220 170'><path d='M30 117 Q17 120 17 101 L18 84 Q21 68 34 78 L50 83 Q59 65 76 64 L95 38 L111 65 Q142 63 158 77 L187 64 L186 101 Q198 116 197 142 L164 136 Q148 146 119 137 L66 123 Z' fill='#f6f4ff' stroke='#101828' stroke-width='5' stroke-linejoin='round'/><path d='M82 65 L93 49 L101 68 M166 80 L179 74 L177 94' fill='#edc7df'/><ellipse cx='96' cy='94' rx='4' ry='5' fill='#172133'/><ellipse cx='150' cy='105' rx='4' ry='5' fill='#172133'/><path d='M111 105 Q114 116 121 108 Q126 121 133 110' fill='none' stroke='#172133' stroke-width='3' stroke-linecap='round'/><g fill='#e8b0ce'><ellipse cx='30' cy='96' rx='5' ry='6'/><circle cx='25' cy='85' r='3'/><circle cx='35' cy='87' r='3'/><circle cx='22' cy='94' r='2'/><ellipse cx='84' cy='103' rx='8' ry='4'/><ellipse cx='159' cy='116' rx='8' ry='4'/></g><path d='M150 143 L157 156 M169 148 L174 161' stroke='#9ccbfb' stroke-width='4' stroke-linecap='round'/></svg>")
    }
    SequentialAnimation on sway {
        running: mascot.visible && mascot.playing && Theme.animations
        loops: Animation.Infinite
        NumberAnimation { to: 1; duration: 650; easing.type: Easing.InOutSine }
        NumberAnimation { to: 0; duration: 650; easing.type: Easing.InOutSine }
    }
}
