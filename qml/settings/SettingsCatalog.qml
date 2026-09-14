import QtQuick

QtObject {
    readonly property var entries: [
        {page:"general", pageName:"General", name:"Interface language", keywords:"language locale Traditional Chinese clock reset first-run guide"},
        {page:"general", pageName:"General", name:"24-hour clock", keywords:"time clock format"},
        {page:"general", pageName:"General", name:"Reset desktop preferences", keywords:"restore defaults reset"},
        {page:"appearance", pageName:"Appearance", name:"Wallpaper", keywords:"background image accent color font panel height"},
        {page:"appearance", pageName:"Appearance", name:"Animations", keywords:"motion blur opacity duration effects"},
        {page:"windows", pageName:"Windows and workspaces", name:"Number of workspaces", keywords:"tiling floating master window gap"},
        {page:"shortcuts", pageName:"Keyboard shortcuts", name:"Global shortcuts", keywords:"keys Meta bind launch focus group workspace close maximize"},
        {page:"modules", pageName:"Shell modules", name:"Custom QML", keywords:"panel overview dashboard JSON modules"},
        {page:"display", pageName:"Display", name:"Nested desktop size", keywords:"monitor resolution scaling HDR night light"},
        {page:"input", pageName:"Keyboard and pointer", name:"Keyboard layout", keywords:"repeat delay cursor input method mouse touchpad"},
        {page:"sound", pageName:"Sound", name:"Output volume", keywords:"microphone mute PipeWire audio devices routing"},
        {page:"network", pageName:"Network", name:"Network connections", keywords:"Wi-Fi Ethernet VPN NetworkManager"},
        {page:"bluetooth", pageName:"Bluetooth", name:"Bluetooth devices", keywords:"pair adapter accessories"},
        {page:"power", pageName:"Power and battery", name:"Power profile", keywords:"battery suspend lid backlight"},
        {page:"applications", pageName:"Applications and startup", name:"Default applications and file associations", keywords:"terminal file manager startup packages plugins X11"},
        {page:"privacy", pageName:"Privacy and accessibility", name:"Reduced motion", keywords:"animations lock screen screen reader plugins user host"},
        {page:"system", pageName:"Users, date and time", name:"User accounts", keywords:"clock authorization accounts"},
        {page:"devices", pageName:"Printers and storage", name:"Printers and scanners", keywords:"disks storage devices"},
        {page:"about", pageName:"About LunaDash", name:"About LunaDash", keywords:"version Wayland graphics system update GitHub Discord community release"}
    ]

    function matches(query, translate) {
        const tokens = query.trim().toLocaleLowerCase().split(/\s+/).filter(Boolean)
        if (!tokens.length)
            return []
        return entries.map((entry, order) => {
            const text = [entry.name, entry.pageName, entry.keywords]
                .map(value => translate(value).toLocaleLowerCase()).join(" ")
            const words = text.split(/[^\p{L}\p{N}]+/u).filter(Boolean)
            let score = 0
            for (const token of tokens) {
                if (words.some(word => word.startsWith(token))) score += 2
                else if (text.includes(token)) score += 1
                else return null
            }
            return {entry: entry, score: score, order: order}
        }).filter(candidate => candidate !== null)
          .sort((left, right) => right.score - left.score || left.order - right.order)
          .map(candidate => candidate.entry)
    }
}
