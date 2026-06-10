#pragma once

#include <QString>
#include <QPixmap>
#include <QSize>

#include "models/RecoverableFile.h"

class PreviewService
{
public:
    bool isImage(const QString& extension) const;
    bool isVideo(const QString& extension) const;

    QPixmap createImagePreview(
        const QString& filePath,
        const QSize& size
    ) const;

    QString createTemporaryPreviewFile(
        const RecoverableFile& file,
        const QString& driveRoot
    ) const;

private:
    QString normalizeDevicePath(
        const QString& driveRoot
    ) const;
};