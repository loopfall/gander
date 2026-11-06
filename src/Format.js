.pragma library

// 3:07, or 1:02:09 past an hour.
function time(ms) {
    if (!(ms > 0))
        return "0:00"
    const total = Math.floor(ms / 1000)
    const h = Math.floor(total / 3600), m = Math.floor((total % 3600) / 60), s = total % 60
    const ss = (s < 10 ? "0" : "") + s
    if (h > 0)
        return h + ":" + (m < 10 ? "0" : "") + m + ":" + ss
    return m + ":" + ss
}

// The theme icon lookup the gander image provider understands: specific name
// first, then the generic one.
function icon(file) {
    if (!file)
        return "image://gander/icon/unknown"
    if (file.kind === "folder")
        return "image://gander/icon/folder"
    return "image://gander/icon/" + (file.iconName || "") + "," + (file.genericIconName || "")
}
