#include "FolderScanner.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QDir>

QVector<RecoverableFile> FolderScanner::scanFolder(
    const QString& folderPath,
    ProgressCallback onProgress,
    CancelCallback isCancelled
)
{
    QVector<RecoverableFile> results;

    QFileInfo rootInfo(folderPath);

    if (!rootInfo.exists() || !rootInfo.isDir())
        return results;

    QDirIterator iterator(
        folderPath,
        QDir::Files | QDir::NoSymLinks | QDir::Readable,
        QDirIterator::Subdirectories
    );

    int count = 0;

    while (iterator.hasNext())
    {
        if (isCancelled && isCancelled())
            break;

        QString path = iterator.next();

        QFileInfo info(path);

        if (!info.exists() || !info.isFile())
            continue;

        QString absolutePath = info.absoluteFilePath();

        if (shouldSkipDirectory(absolutePath))
            continue;

        RecoverableFile file;

        file.originalName = info.fileName();
        file.originalPath = absolutePath;
        file.currentPath = absolutePath;
        file.extension = info.suffix().toLower();
        file.size = info.size();
        file.deletedAt = info.lastModified();
        file.source = RecoverySource::Folder;

        results.push_back(file);

        count++;

        if (onProgress && count % 250 == 0)
        {
            int progress = qMin(99, count / 250);
            onProgress(progress);
        }
    }

    if (onProgress)
        onProgress(100);

    return results;
}

bool FolderScanner::shouldSkipDirectory(
    const QString& path
) const
{
    QString normalized = QDir::fromNativeSeparators(path).toLower();

    static const QStringList blockedParts = {
        "/windows/",
        "/program files/",
        "/program files (x86)/",
        "/programdata/",
        "/system volume information/",
        "/$recycle.bin/",
        "/appdata/local/temp/",
        "/appdata/local/microsoft/windows/",
        "/appdata/local/packages/",
        "/node_modules/",
        "/.git/",
        "/build/",
        "/debug/",
        "/release/"
    };

    for (const QString& blocked : blockedParts)
    {
        if (normalized.contains(blocked))
            return true;
    }

    return false;
}