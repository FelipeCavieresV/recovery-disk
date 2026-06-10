#pragma once

#include <QString>
#include <QVector>
#include <QSet>
#include <functional>

#include "models/RecoverableFile.h"

class FolderScanner
{
public:
    using ProgressCallback = std::function<void(int)>;
    using CancelCallback = std::function<bool()>;

    QVector<RecoverableFile> scanFolder(
        const QString& folderPath,
        ProgressCallback onProgress = nullptr,
        CancelCallback isCancelled = nullptr
    );

private:
    bool shouldSkipDirectory(
        const QString& path
    ) const;
};