#pragma once

#include <QString>
#include <QDateTime>
#include <QVector>
#include <QMetaType>

enum class RecoverySource
{
    RecycleBin,
    Folder,
    DeepScan
};

enum class RecoveryQuality
{
    Good,
    Regular,
    Bad,
    Unknown
};

struct RecoverableFile
{
    QString originalName;
    QString originalPath;
    QString currentPath;
    QString extension;

    qint64 size = 0;
    QDateTime deletedAt;

    RecoverySource source = RecoverySource::Folder;
    RecoveryQuality quality = RecoveryQuality::Unknown;

    qint64 offset = -1;
    bool hasValidHeader = false;
    bool hasValidFooter = false;
};

Q_DECLARE_METATYPE(RecoverableFile)
Q_DECLARE_METATYPE(QVector<RecoverableFile>)