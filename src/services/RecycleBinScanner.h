#pragma once

#include <QVector>
#include <QString>

#include "models/RecoverableFile.h"

class RecycleBinScanner
{
public:
    QVector<RecoverableFile> scanAllDrives();

    QVector<RecoverableFile> scanDriveRecycleBin(
        const QString& driveRoot
    );

private:
    QVector<RecoverableFile> scanRecycleBinPath(
        const QString& recycleBinPath
    );

    QString buildDataFilePath(
        const QString& infoFilePath
    ) const;

    RecoverableFile parseInfoFile(
        const QString& infoFilePath,
        const QString& dataFilePath
    ) const;
};