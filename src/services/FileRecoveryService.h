#pragma once

#include <QString>
#include <QVector>

#include "models/RecoverableFile.h"

struct RecoveryResult
{
    int total = 0;
    int recovered = 0;
    int failed = 0;

    QString outputFolder;
    QString reportPath;

    QVector<QString> recoveredFiles;
    QVector<QString> failedFiles;
};

class FileRecoveryService
{
public:
    RecoveryResult recoverFiles(
        const QVector<RecoverableFile>& files,
        const QString& destinationFolder
    );

private:
    QString createRecoveryFolder(
        const QString& destinationFolder
    ) const;

    QString safeFileName(
        const QString& fileName
    ) const;

    bool copySingleFile(
        const RecoverableFile& file,
        const QString& outputFolder,
        QString& outputPath,
        QString& error
    ) const;

    QString createReport(
        const RecoveryResult& result
    ) const;
};