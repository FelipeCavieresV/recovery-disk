#include "FileRecoveryService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QTextStream>

RecoveryResult FileRecoveryService::recoverFiles(
    const QVector<RecoverableFile>& files,
    const QString& destinationFolder,
    const QString& driveRoot
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

        bool ok = false;

        if (file.source == RecoverySource::DeepScan && !driveRoot.isEmpty())
        {
            QString baseName = safeFileName(file.originalName);
            outputPath = QDir(result.outputFolder).filePath(baseName);

            QFileInfo outputInfo(outputPath);
            int counter = 1;

            while (outputInfo.exists())
            {
                QString stem = outputInfo.completeBaseName();
                QString suffix = outputInfo.suffix();
                QString newName = suffix.isEmpty()
                    ? QString("%1_%2").arg(stem).arg(counter)
                    : QString("%1_%2.%3").arg(stem).arg(counter).arg(suffix);
                outputPath = QDir(result.outputFolder).filePath(newName);
                outputInfo = QFileInfo(outputPath);
                counter++;
            }

            if (!file.currentPath.isEmpty() && QFileInfo::exists(file.currentPath))
                ok = QFile::copy(file.currentPath, outputPath);
            else
                ok = extractFromDisk(file, driveRoot, outputPath, error);

            if (ok)
            {
                result.recovered++;
                result.recoveredFiles.push_back(outputPath);
            }
            else
            {
                result.failed++;
                result.failedFiles.push_back(file.originalName + " | " + error);
            }
        }
        else
        {
            ok = copySingleFile(file, result.outputFolder, outputPath, error);

            if (ok)
            {
                result.recovered++;
                result.recoveredFiles.push_back(outputPath);
            }
            else
            {
                result.failed++;
                result.failedFiles.push_back(file.originalName + " | " + error);
            }
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
        "DemogoRecovery_Recovered_" + timestamp
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

    out << "DemogoRecovery - Reporte de recuperación\n";
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

QString FileRecoveryService::normalizeDrivePath(const QString& driveRoot) const
{
    QString drive = driveRoot.trimmed();
    drive.replace("/", "\\");

    if (drive.startsWith("\\\\.\\"))
        return drive;

    if (drive.endsWith("\\"))
        drive.chop(1);

    if (drive.endsWith(":"))
        return "\\\\.\\" + drive.toUpper();

    if (drive.length() == 1)
        return "\\\\.\\" + drive.toUpper() + ":";

    return drive;
}

bool FileRecoveryService::extractFromDisk(
    const RecoverableFile& file,
    const QString& driveRoot,
    const QString& outputPath,
    QString& error
) const
{
    if (file.offset < 0)
    {
        error = "Offset inválido";
        return false;
    }

    const QString devicePath = normalizeDrivePath(driveRoot);

    QFile drive(devicePath);

    if (!drive.open(QIODevice::ReadOnly))
    {
        error = "No se pudo abrir la unidad (ejecuta como administrador)";
        return false;
    }

    if (!drive.seek(file.offset))
    {
        drive.close();
        error = "No se pudo buscar el offset en la unidad";
        return false;
    }

    // Tamaño total a extraer. Para archivos reconstruidos conocemos el tamaño
    // exacto; si no, usamos un valor por defecto moderado.
    const qint64 totalBytes = file.size > 0
        ? file.size
        : 50LL * 1024 * 1024;

    QFile out(outputPath);

    if (!out.open(QIODevice::WriteOnly))
    {
        drive.close();
        error = "No se pudo crear el archivo de salida";
        return false;
    }

    // Copia en streaming por bloques alineados a sector (Windows exige lecturas
    // de volumen crudo con tamaño múltiplo del sector). Escribimos exactamente
    // el tamaño del archivo. No carga todo en memoria: sirve para videos de GB.
    const qint64 S = 512;
    const qint64 chunkSize = 4LL * 1024 * 1024;  // múltiplo del sector

    qint64 remaining = totalBytes;
    qint64 pos = file.offset;
    qint64 written = 0;

    while (remaining > 0)
    {
        const qint64 want = qMin(chunkSize, remaining);
        const qint64 toRead = ((want + S - 1) / S) * S;  // alinear hacia arriba

        if (!drive.seek(pos))
            break;

        QByteArray chunk = drive.read(toRead);

        if (chunk.isEmpty())
            break;  // fin del dispositivo

        const qint64 writeLen = qMin<qint64>(chunk.size(), remaining);

        const qint64 w = out.write(chunk.constData(), writeLen);

        if (w < 0)
        {
            out.close();
            drive.close();
            error = "Error al escribir el archivo de salida";
            return false;
        }

        written += w;
        remaining -= writeLen;
        pos += chunk.size();
    }

    out.close();
    drive.close();

    if (written <= 0)
    {
        error = "No se pudieron leer datos del disco";
        QFile::remove(outputPath);
        return false;
    }

    return true;
}