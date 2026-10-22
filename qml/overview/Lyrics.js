.pragma library

function parse(text) {
    const rows = []
    const plain = []
    let offset = 0
    for (const line of String(text || "").split(/\r?\n/).slice(0, 2000)) {
        const adjustment = line.match(/^\[offset:([+-]?\d+)\]/i)
        if (adjustment) { offset = Number(adjustment[1]) / 1000; continue }
        const timestamps = []
        const expression = /\[(\d+):(\d{2})(?:[.:](\d{1,3}))?\]/g
        let match
        while ((match = expression.exec(line)) !== null)
            timestamps.push(match)
        const content = line.replace(/\[[^\]]*\]/g, "").trim()
        if (timestamps.length) {
            for (const stamp of timestamps) {
                const time = Number(stamp[1]) * 60 + Number(stamp[2]) + Number("0." + (stamp[3] || "0"))
                if (Number(stamp[2]) < 60 && content.length)
                    rows.push({time: time, text: content})
            }
        } else if (content.length) {
            plain.push(content)
        }
    }
    rows.sort((a, b) => a.time - b.time)
    return {timed: rows.map(row => ({time: Math.max(0, row.time - offset), text: row.text})), plain: plain.join("\n")}
}

function currentIndex(rows, seconds) {
    let index = -1
    for (let i = 0; i < rows.length && rows[i].time <= seconds; ++i)
        index = i
    return index
}

function clock(seconds) {
    const value = Math.max(0, Math.floor(Number(seconds) || 0))
    return Math.floor(value / 60) + ":" + String(value % 60).padStart(2, "0")
}
