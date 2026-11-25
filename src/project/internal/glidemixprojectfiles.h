/*
 * GlideMix: moving a saved project on disk -- the .aup4 file and the SQLite
 * side files that travel with it (a -wal file can hold the latest changes).
 * Used by Home's Rename/Delete (RecentProjectsModel) and by the Project tab's
 * menu for the open project (ProjectActionsController).
 */
#pragma once

#include <QFile>
#include <QFileInfo>
#include <QString>

namespace au::project::glidemix {
inline const char* const kProjectSideSuffixes[] = { "-wal", "-shm", "-journal" };

//! Moves the project file and its side files to the Trash. False when the
//! project file itself could not be moved.
inline bool trashProjectFiles(const QString& path)
{
    if (QFileInfo::exists(path) && !QFile::moveToTrash(path)) {
        return false;
    }
    for (const char* suffix : kProjectSideSuffixes) {
        const QString side = path + QString::fromUtf8(suffix);
        if (QFileInfo::exists(side)) {
            QFile::moveToTrash(side);
        }
    }
    return true;
}

//! Renames the project file and its side files. False when the project file
//! itself could not be renamed (nothing else moved then).
inline bool renameProjectFiles(const QString& from, const QString& to)
{
    if (!QFile::rename(from, to)) {
        return false;
    }
    for (const char* suffix : kProjectSideSuffixes) {
        const QString side = from + QString::fromUtf8(suffix);
        if (QFileInfo::exists(side)) {
            QFile::rename(side, to + QString::fromUtf8(suffix));
        }
    }
    return true;
}

//! The path the user picked in a save panel, with the .aup4 extension.
inline QString withProjectSuffix(const QString& picked)
{
    const QFileInfo info(picked);
    if (info.suffix().compare(u"aup4", Qt::CaseInsensitive) == 0) {
        return picked;
    }
    return info.absolutePath() + u'/' + info.completeBaseName() + QStringLiteral(".aup4");
}
}
