#pragma once

#include <QString>
#include <QList>

enum class DriveType
{
    Unknown,
    Fixed,
    Removable,
    Network,
    CdRom,
    Ram
};

struct DriveInfo
{
    QString name;
    QString path;
    QString fileSystem;
    QString displayLabel;
    qint64 totalBytes = 0;
    qint64 freeBytes = 0;
    bool isReady = false;
    DriveType driveType = DriveType::Unknown;
};

class DriveScanner
{
public:
    static QList<DriveInfo> getAvailableDrives();
    static QString formatBytes(qint64 bytes);
};