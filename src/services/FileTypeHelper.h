#pragma once

#include <QString>
#include <QStringList>

#include "models/RecoverableFile.h"

class FileTypeHelper
{
public:
    static QString typeKey(const QString& extension);
    static QString typeName(const QString& extension);

    static QStringList categoryKeys();
    static QString categoryTitle(const QString& key);

    static RecoveryQuality recoveryQuality(
        const RecoverableFile& file
    );

    static QString recoveryQualityText(
        const RecoverableFile& file
    );
};