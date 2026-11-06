#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>

// The first, synchronous look at a file: everything the window needs to size
// itself and paint something on the very first frame (kind, name, image
// dimensions, the thumbnail Nautilus already made). It only stats the file and
// reads headers, so it stays in the low milliseconds; anything heavier runs
// later, off the GUI thread.
namespace Inspector {

enum class Kind { Missing, Folder, Image, Animated, Video, Audio, Pdf, Office, Table, Markdown, Text, Font, Archive, Generic };

QString kindName(Kind kind);

// Picks the viewer from a MIME type and its ancestors. imageReadable says
// whether Qt has an image plugin for it (RAW, PSD and friends fall back to
// the generic view, which still shows their thumbnail).
Kind classify(const QString &mime, const QStringList &ancestors, bool imageReadable);

QVariantMap inspect(const QString &path);

// file:// URI escaped exactly like GLib's g_filename_to_uri(), because the
// freedesktop thumbnail cache is keyed by the MD5 of that exact string.
QString fileUri(const QString &absolutePath);

// The largest valid thumbnail for the file in the freedesktop cache, as a
// file URL, or empty. A thumbnail whose Thumb::MTime disagrees is stale.
QString thumbnailFor(const QString &absolutePath, qint64 mtime, const QString &cacheRoot = {});

// Nautilus hands over URIs; trash://, recent:// and gvfs mounts resolve to a
// local path through GIO's own answer.
QString localPathForUri(const QString &uri);

// Text that only looks like an unknown binary type (no extension, odd
// extension) is still worth showing as text.
bool looksLikeText(const QByteArray &head);

} // namespace Inspector
