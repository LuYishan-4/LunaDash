.pragma library

function desktopId(value) {
    return String(value || "").trim().replace(/\.desktop$/i, "")
}

function desktopEntry(appId, entries) {
    const id = desktopId(appId)
    if (!id) return null
    const exact = entries.filter(entry => desktopId(entry.id) === id)
    if (exact.length === 1) return exact[0]
    const folded = id.toLowerCase()
    const byId = entries.filter(entry => desktopId(entry.id).toLowerCase() === folded)
    if (byId.length === 1) return byId[0]
    const byClass = entries.filter(entry => String(entry.startupClass || "").toLowerCase() === folded)
    return byClass.length === 1 ? byClass[0] : null
}

function isThemeIconName(name) {
    const value = String(name || "").trim()
    return value.length > 0 && value !== "lunadash" && !value.includes("/")
        && !/\.(png|jpe?g|webp|svg|xpm|ico)$/i.test(value)
}

function fileSource(name) {
    const value = String(name || "")
    if (/^(image:|file:|qrc:|data:)/.test(value) && !value.includes("qs-blackhole")) return value
    if (value.startsWith("/") && /\.(png|jpe?g|webp|svg|xpm)$/i.test(value)) return "file://" + value
    return ""
}

function vectorName(iconName) {
    switch (iconName) {
    case "system-file-manager": return "files"
    case "preferences-system":
    case "preferences-desktop-theme":
    case "preferences-desktop-emoticons": return "settings"
    case "utilities-terminal": return "terminal"
    case "utilities-system-monitor":
    case "hwinfo": return "monitor"
    default: return "apps"
    }
}
