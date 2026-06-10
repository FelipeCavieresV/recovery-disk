#include "RecycleBinScanner.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>
#include <QDirIterator>
#include <QDataStream>
#include <QDateTime>

static QDateTime windowsFileTimeToDateTime(qint64 fileTime)
{
    const qint64 windowsTicksPerSecond = 10000000;
    const qint64 secondsBetween1601And1970 = 11644473600LL;

    qint64 seconds =
        fileTime / windowsTicksPerSecond - secondsBetween1601And1970;

    return QDateTime::fromSecsSinceEpoch(seconds);
}

QVector<RecoverableFile> RecycleBinScanner::scanAllDrives()
{
    QVector<RecoverableFile> results;

    const QList<QStorageInfo> drives = QStorageInfo::mountedVolumes();

    for (const QStorageInfo& drive : drives)
    {
        if (!drive.isValid() || !drive.isReady())
        {
            continue;
        }

        results.append(
            scanDriveRecycleBin(drive.rootPath())
        );
    }

    return results;
}

QVector<RecoverableFile> RecycleBinScanner::scanDriveRecycleBin(
    const QString& driveRoot
)
{
    QString root = driveRoot;
    root.replace("\\", "/");

    if (!root.endsWith("/"))
    {
        root += "/";
    }

    QString recycleBinPath = root + "$Recycle.Bin";

    return scanRecycleBinPath(recycleBinPath);
}

QVector<RecoverableFile> RecycleBinScanner::scanRecycleBinPath(
    const QString& recycleBinPath
)
{
    QVector<RecoverableFile> results;

    QDir recycleDir(recycleBinPath);

    if (!recycleDir.exists())
    {
        return results;
    }

    QDirIterator iterator(
        recycleBinPath,
        QStringList() << "$I*",
        QDir::Files | QDir::Hidden | QDir::System,
        QDirIterator::Subdirectories
    );

    while (iterator.hasNext())
    {
        QString infoFilePath = iterator.next();
        QString dataFilePath = buildDataFilePath(infoFilePath);

        if (dataFilePath.isEmpty())
        {
            continue;
        }

        if (!QFileInfo::exists(dataFilePath))
        {
            continue;
        }

        RecoverableFile file =
            parseInfoFile(infoFilePath, dataFilePath);

        if (!file.currentPath.isEmpty())
        {
            results.append(file);
        }
    }

    return results;
}

QString RecycleBinScanner::buildDataFilePath(
    const QString& infoFilePath
) const
{
    QFileInfo info(infoFilePath);

    QString fileName = info.fileName();

    if (!fileName.startsWith("$I"))
    {
        return QString();
    }

    fileName.replace(0, 2, "$R");

    return info.absolutePath() + "/" + fileName;
}

RecoverableFile RecycleBinScanner::parseInfoFile(
    const QString& infoFilePath,
    const QString& dataFilePath
) const
{
    RecoverableFile result;

    QFile infoFile(infoFilePath);

    if (!infoFile.open(QIODevice::ReadOnly))
    {
        return result;
    }

    QByteArray bytes = infoFile.readAll();
    infoFile.close();

    if (bytes.size() < 24)
    {
        return result;
    }

    QDataStream stream(bytes);
    stream.setByteOrder(QDataStream::LittleEndian);

    qint64 version = 0;
    qint64 originalSize = 0;
    qint64 deletedFileTime = 0;

    stream >> version;
    stream >> originalSize;
    stream >> deletedFileTime;

    QByteArray pathBytes = bytes.mid(24);

    QString originalPath =
        QString::fromUtf16(
            reinterpret_cast<const char16_t*>(pathBytes.constData()),
            pathBytes.size() / 2
        );

    originalPath = originalPath.trimmed();
    originalPath.remove(QChar('\0'));

    QFileInfo originalInfo(originalPath);
    QFileInfo dataInfo(dataFilePath);

    result.originalName = originalInfo.fileName();

    if (result.originalName.isEmpty())
    {
        result.originalName = dataInfo.fileName();
    }

    result.originalPath = originalPath;
    result.currentPath = dataFilePath;
    result.extension = originalInfo.suffix().toLower();

    if (result.extension.isEmpty())
    {
        result.extension = dataInfo.suffix().toLower();
    }

    result.size = originalSize;

    if (result.size <= 0)
    {
        result.size = dataInfo.size();
    }

    result.deletedAt = windowsFileTimeToDateTime(deletedFileTime);
    result.source = RecoverySource::RecycleBin;

    return result;
}