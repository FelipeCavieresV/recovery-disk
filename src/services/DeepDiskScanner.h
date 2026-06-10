#pragma once

#include <QString>
#include <QVector>
#include <QByteArray>
#include <functional>
#ifdef Q_OS_WIN
#include <windows.h>
#include <winioctl.h>
#endif
#include "models/RecoverableFile.h"
#include "signatures/FileSignature.h"

class DeepDiskScanner
{
public:
    using ProgressCallback = std::function<void(int)>;
    using BatchCallback = std::function<void(const QVector<RecoverableFile>&)>;
    using CancelCallback = std::function<bool()>;

    QVector<RecoverableFile> scanDrive(
        const QString& driveLetter,
        ProgressCallback onProgress = nullptr,
        BatchCallback onBatch = nullptr,
        CancelCallback isCancelled = nullptr,
        QString* errorMessage = nullptr
    );

private:
    QString normalizeDrivePath(const QString& driveLetter) const;

    qint64 getDeviceSize(
    const QString& driveLetter,
    const QString& devicePath
    ) const;

    QString normalizeRootPath(
        const QString& driveLetter
    ) const;
};