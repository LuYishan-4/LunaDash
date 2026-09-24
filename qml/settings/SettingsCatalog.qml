import QtQuick

QtObject {
    readonly property var entries: [
        {page:"general", pageName:"General", name:"Interface language", keywords:"language locale Traditional Chinese clock reset first-run guide"},
        {page:"general", pageName:"General", name:"24-hour clock", keywords:"time clock format"},
        {page:"general", pageName:"General", name:"Notifications", keywords:"desktop notification crash alerts toast details"},
        {page:"general", pageName:"General", name:"Reset desktop preferences", keywords:"restore defaults reset"},
        {page:"appearance", pageName:"Appearance", name:"Wallpaper", keywords:"background image video live library category search random accent color font panel height"},
        {page:"appearance", pageName:"Appearance", name:"Visual effects", keywords:"animation blur opacity duration effects"},
        {page:"appearance", pageName:"Appearance", name:"Colors and comfort", keywords:"light dark automatic theme material wallpaper palette eye care night temperature GTK Kitty dock presets"},
        {page:"appearance", pageName:"Appearance", name:"Orbit launcher", keywords:"radial apps folders links AI search engines JSON custom scratchpad"},
        {page:"windows", pageName:"Windows and workspaces", name:"Number of workspaces", keywords:"tiling floating master window gap resize guide Alt"},
        {page:"shortcuts", pageName:"Keyboard shortcuts", name:"Global shortcuts", keywords:"keys Meta bind launch focus group workspace close maximize"},
        {page:"plugins", pageName:"Plugins", name:"Desktop extensions", keywords:"plugins addons native Quickshell OpenGL effect hooks enable disable settings JSON stacking window layout animation"},
        {page:"modules", pageName:"Shell modules", name:"Module layout", keywords:"panel overview dashboard JSON size position color"},
        {page:"dashboard", pageName:"Dashboard", name:"Dashboard cards", keywords:"overview volume Wi-Fi media player MPRIS Spotify YouTube VLC calendar artwork cover shortcuts quick launch"},
        {page:"dashboard", pageName:"Dashboard", name:"Quick launch", keywords:"files terminal settings monitor network plugins customize"},
        {page:"display", pageName:"Display", name:"Nested desktop size", keywords:"monitor resolution scaling HDR night light"},
        {page:"input", pageName:"Keyboard and pointer", name:"Keyboard layout", keywords:"repeat delay cursor mouse touchpad"},
        {page:"input-method", pageName:"Input method", name:"Fcitx 5", keywords:"input method IME fcitx fcitx5 preedit candidate Chinese Japanese Korean addon hotkey"},
        {page:"sound", pageName:"Sound", name:"Output volume", keywords:"microphone mute PipeWire audio devices routing"},
        {page:"network", pageName:"Internet and network", name:"Network connections", keywords:"Wi-Fi Ethernet VPN NetworkManager adapters connect disconnect"},
        {page:"network", pageName:"Internet and network", name:"IPv4, IPv6 and DNS", keywords:"IP address gateway DNS DHCP metric resolver"},
        {page:"network", pageName:"Internet and network", name:"Internet Options", keywords:"General Security Privacy Content Connections Programs Advanced proxy firewall certificates TLS default browser diagnostics routes"},
        {page:"bluetooth", pageName:"Bluetooth", name:"Bluetooth devices", keywords:"pair adapter accessories"},
        {page:"devices", pageName:"Device manager and disks", name:"Device Manager", keywords:"hardware PCI USB driver kernel modules status details events scan hidden device manager"},
        {page:"devices", pageName:"Device manager and disks", name:"Disk Management", keywords:"disk partition volume mount unmount format initialize GPT MBR resize SMART filesystem label UUID eject storage"},
        {page:"power", pageName:"Power and battery", name:"Power profile", keywords:"battery suspend lid backlight power device"},
        {page:"applications", pageName:"Applications and startup", name:"Default applications and file associations", keywords:"terminal file manager startup packages plugins X11 portal file chooser browser programs associations"},
        {page:"privacy", pageName:"Privacy and accessibility", name:"Reduced motion", keywords:"animations lock screen screen reader plugins user host permissions privacy security"},
        {page:"system", pageName:"Users, date and time", name:"User accounts", keywords:"clock authorization accounts"},
        {page:"about", pageName:"About LunaDash", name:"Software updates", keywords:"version Wayland graphics system update rollback stable dev commit release GitHub"}
    ]

    function matches(query, translate) {
        const tokens = query.trim().toLocaleLowerCase().split(/\s+/).filter(Boolean)
        if (!tokens.length)
            return []
        return entries.map((entry, order) => {
            const text = [entry.name, entry.pageName, entry.keywords]
                .map(value => (String(value) + " " + translate(value)).toLocaleLowerCase()).join(" ")
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
