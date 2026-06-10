#pragma once

#include <QString>
#include <QList>

struct DriveInfo
{
    QString name;
    QString path;
    QString fileSystem;
    qint64 totalBytes = 0;
    qint64 freeBytes = 0;
    bool isReady = false;
};

class DriveScanner
{
public:
    static QList<DriveInfo> getAvailableDrives();
    static QString formatBytes(qint64 bytes);
};