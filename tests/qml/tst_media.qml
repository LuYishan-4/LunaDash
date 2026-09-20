import QtQuick
import QtTest
import "../../qml/overview"
import "../../qml/overview/Lyrics.js" as Lyrics

TestCase {
    id: test
    name: "Media"
    visible: true
    when: windowShown
    width: 1000
    height: 540
    QtObject { id: backendShell; function tr(text) { return text } }
    Component { id: component; MediaView { width: test.width; height: test.height; shell: backendShell } }
    function test_timed_lyrics_offsets_and_plain_text() {
        const parsed = Lyrics.parse("[ar:Artist]\n[offset:250]\n[00:01.00][00:05.500]Repeated line\n[00:03.25]Middle")
        compare(parsed.timed.length, 3)
        compare(parsed.timed[0].time, 0.75)
        compare(parsed.timed[1].time, 3)
        compare(parsed.timed[2].time, 5.25)
        compare(Lyrics.currentIndex(parsed.timed, 0), -1)
        compare(Lyrics.currentIndex(parsed.timed, 4), 1)
        compare(Lyrics.parse("First\nSecond").plain, "First\nSecond")
        compare(Lyrics.clock(93.8), "1:33")
    }
    function test_media_layout_and_paused_progress() {
        const item = createTemporaryObject(component, test)
        verify(item !== null)
        item.media = {available: true, title: "A long title that stays inside the player", artist: "Example artist",
            identity: "Example player", playing: false, positionSupported: true, positionUs: 32000000,
            lengthUs: 180000000, canPlay: true, canPause: true, lyrics: "[00:01]One\n[00:30]Two",
            players: [{service: "org.mpris.MediaPlayer2.example", bus: "session", identity: "Example player"}]}
        compare(item.position, 32)
        compare(item.lyricIndex, 1)
        wait(300)
        compare(item.position, 32)
        compare(grabImage(item).width, item.width)
        item.width = 620
        compare(grabImage(item).width, item.width)
        compare(item.players.length, 1)
    }
}
