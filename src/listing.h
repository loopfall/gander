#pragma once

#include <QStringList>
#include <QVariantMap>

namespace Listing {

// A folder's entries for the preview: folders first, then files in natural
// order (track 2 before track 10), hidden entries left out. Carries entries
// (name, icon, size, folder), count and fileBytes. Runs in a worker thread.
QVariantMap folder(const QString &path, int maxEntries = 400);

// Bytes under a folder, recursively, until the deadline; partial says the
// walk stopped early and the number is a lower bound.
QVariantMap folderSize(const QString &path, int deadlineMs = 1500);

// An archive's table of contents through bsdtar, which reads zip, tar in all
// its compressions, 7z, rar, iso, cpio, deb and rpm.
QVariantMap archive(const QString &path, int maxEntries = 600);

// One line of `bsdtar -tv` output: name, bytes and whether it's a folder.
bool parseArchiveLine(const QString &line, QString *name, qint64 *bytes, bool *folder);

// The paths a preview steps through with the arrow keys, in the same order
// the folder listing uses.
QStringList siblings(const QString &directory, bool includeHidden);

} // namespace Listing
