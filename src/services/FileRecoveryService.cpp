#include "FileRecoveryService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QTextStream>

RecoveryResult FileRecoveryService::recoverFiles(
    const QVector<RecoverableFile>& files,
    const QString& destinationFolder
)
{
    RecoveryResult result;

    result.total = files.size();
    result.outputFolder = createRecoveryFolder(destinationFolder);

    QDir().mkpath(result.outputFolder);

    for (const RecoverableFile& file : files)
    {
        QString outputPath;
        QString error;

        if (copySingleFile(file, result.outputFolder, outputPath, error))
        {
            result.recovered++;
            result.recoveredFiles.push_back(outputPath);
        }
        else
        {
            result.failed++;
            result.failedFiles.push_back(
                file.originalName + " | " + error
            );
        }
    }

    result.reportPath = createReport(result);

    return result;
}

QString FileRecoveryService::createRecoveryFolder(
    const QString& destinationFolder
) const
{
    QString timestamp =
        QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");

    return QDir(destinationFolder).filePath(
        "RecoveryDisk_Recovered_" + timestamp
    );
}

QString FileRecoveryService::safeFileName(
    const QString& fileName
) const
{
    QString safe = fileName;

    safe.replace("\\", "_");
    safe.replace("/", "_");
    safe.replace(":", "_");
    safe.replace("*", "_");
    safe.replace("?", "_");
    safe.replace("\"", "_");
    safe.replace("<", "_");
    safe.replace(">", "_");
    safe.replace("|", "_");

    if (safe.trimmed().isEmpty())
        safe = "recovered_file";

    return safe;
}

bool FileRecoveryService::copySingleFile(
    const RecoverableFile& file,
    const QString& outputFolder,
    QString& outputPath,
    QString& error
) const
{
    if (file.currentPath.isEmpty())
    {
        error = "Ruta actual vacía";
        return false;
    }

    QFileInfo sourceInfo(file.currentPath);

    if (!sourceInfo.exists() || !sourceInfo.isFile())
    {
        error = "Archivo origen no existe";
        return false;
    }

    QString baseName = safeFileName(file.originalName);

    outputPath = QDir(outputFolder).filePath(baseName);

    QFileInfo outputInfo(outputPath);

    int counter = 1;

    while (outputInfo.exists())
    {
        QString stem = outputInfo.completeBaseName();
        QString suffix = outputInfo.suffix();

        QString newName;

        if (suffix.isEmpty())
        {
            newName =
                QString("%1_%2")
                    .arg(stem)
                    .arg(counter);
        }
        else
        {
            newName =
                QString("%1_%2.%3")
                    .arg(stem)
                    .arg(counter)
                    .arg(suffix);
        }

        outputPath = QDir(outputFolder).filePath(newName);
        outputInfo = QFileInfo(outputPath);

        counter++;
    }

    if (!QFile::copy(file.currentPath, outputPath))
    {
        error = "No se pudo copiar el archivo";
        return false;
    }

    return true;
}

QString FileRecoveryService::createReport(
    const RecoveryResult& result
) const
{
    QString reportPath =
        QDir(result.outputFolder).filePath("recovery_report.txt");

    QFile report(reportPath);

    if (!report.open(QIODevice::WriteOnly | QIODevice::Text))
        return "";

    QTextStream out(&report);

    out << "RecoveryDisk - Reporte de recuperación\n";
    out << "Fecha: "
        << QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm:ss")
        << "\n\n";

    out << "Total seleccionados: " << result.total << "\n";
    out << "Recuperados: " << result.recovered << "\n";
    out << "Fallidos: " << result.failed << "\n";
    out << "Carpeta destino: " << result.outputFolder << "\n\n";

    out << "=== Archivos recuperados ===\n";

    for (const QString& path : result.recoveredFiles)
        out << path << "\n";

    out << "\n=== Archivos fallidos ===\n";

    for (const QString& item : result.failedFiles)
        out << item << "\n";

    report.close();

    return reportPath;
}