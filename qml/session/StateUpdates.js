.pragma library

// Keep invisible telemetry from invalidating every shell binding. Preserve
// geometry, mode, errors and pending display changes even when metrics sleep.
function prepare(result, previous, liveMetrics) {
    if (result.translations === undefined && result.language === previous.language)
        result.translations = previous.translations || {}
    if (liveMetrics)
        return result
    if (previous.system !== undefined)
        result.system = previous.system
    if (result.display && previous.display) {
        const display = Object.assign({}, result.display)
        for (const key of ["frameCallbacks", "frameWorkMs", "maxFrameWorkMs", "slowFrames", "eventLoop"])
            if (previous.display[key] !== undefined)
                display[key] = previous.display[key]
        result.display = display
    }
    return result
}
