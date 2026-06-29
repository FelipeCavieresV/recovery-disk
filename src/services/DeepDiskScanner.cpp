#include "DeepDiskScanner.h"

#include "signatures/SignatureDatabase.h"
#include "signatures/SignatureScanner.h"
#include "filesystem/FatRecovery.h"

#include <QFile>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

QString DeepDiskScanner::normalizeDrivePath(
    const QString& driveLetter
) const
{
    QString drive = driveLetter.trimmed();
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

QString DeepDiskScanner::normalizeRootPath(
    const QString& driveLetter
) const
{
    QString drive = driveLetter.trimmed();
    drive.replace("/", "\\");

    if (drive.startsWith("\\\\.\\"))
    {
        drive.remove("\\\\.\\");
    }

    if (drive.endsWith("\\"))
        drive.chop(1);

    if (drive.length() == 1)
        return drive.toUpper() + ":\\";

    if (drive.endsWith(":"))
        return drive.toUpper() + "\\";

    return drive.left(2).toUpper() + "\\";
}

qint64 DeepDiskScanner::getDeviceSize(
    const QString& driveLetter,
    const QString& devicePath
) const
{
#ifdef Q_OS_WIN
    HANDLE handle = CreateFileW(
        reinterpret_cast<LPCWSTR>(devicePath.utf16()),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (handle != INVALID_HANDLE_VALUE)
    {
        GET_LENGTH_INFORMATION lengthInfo;
        DWORD bytesReturned = 0;

        BOOL ok = DeviceIoControl(
            handle,
            IOCTL_DISK_GET_LENGTH_INFO,
            nullptr,
            0,
            &lengthInfo,
            sizeof(lengthInfo),
            &bytesReturned,
            nullptr
        );

        CloseHandle(handle);

        if (ok)
            return static_cast<qint64>(lengthInfo.Length.QuadPart);
    }

    QString rootPath = normalizeRootPath(driveLetter);

    ULARGE_INTEGER freeBytesAvailable;
    ULARGE_INTEGER totalBytes;
    ULARGE_INTEGER totalFreeBytes;

    BOOL ok = GetDiskFreeSpaceExW(
        reinterpret_cast<LPCWSTR>(rootPath.utf16()),
        &freeBytesAvailable,
        &totalBytes,
        &totalFreeBytes
    );

    if (ok)
        return static_cast<qint64>(totalBytes.QuadPart);
#endif

    return 0;
}

QVector<RecoverableFile> DeepDiskScanner::scanDrive(
    const QString& driveLetter,
    ProgressCallback onProgress,
    BatchCallback onBatch,
    CancelCallback isCancelled,
    QString* errorMessage
)
{
    QVector<RecoverableFile> results;

    const QString devicePath = normalizeDrivePath(driveLetter);

    // ── Paso 1: intentar recuperación por tabla del sistema de archivos ──
    // Es lo que permite reconstruir archivos con su nombre/ruta/fecha reales
    // (igual que la sección "Reconstruido" de 4DDiG). Si el volumen no es FAT,
    // caemos al carving por firmas.
    {
        FatRecovery fatRecovery;
        QString fatError;

        QVector<RecoverableFile> fatResults =
            fatRecovery.recover(
                devicePath,
                onProgress,
                onBatch,
                isCancelled,
                &fatError
            );

        const bool notFat = (fatError == "__NOT_FAT__");

        if (!notFat)
        {
            // Era un volumen FAT: devolvemos lo reconstruido (aunque esté vacío).
            if (!fatError.isEmpty() && errorMessage)
                *errorMessage = fatError;

            return fatResults;
        }
        // Si no es FAT, continuamos con el carving por firmas más abajo.
    }

    QFile drive(devicePath);

    if (!drive.open(QIODevice::ReadOnly))
    {
        if (errorMessage)
        {
            *errorMessage =
                "No se pudo abrir la unidad:\n" +
                devicePath +
                "\n\nEjecuta DemogoRecovery como administrador.";
        }

        return results;
    }

    const qint64 totalBytes = getDeviceSize(driveLetter, devicePath);

    if (totalBytes <= 0)
    {
        if (errorMessage)
        {
            *errorMessage =
                "No se pudo obtener el tamaño real de la unidad:\n" +
                devicePath;
        }

        drive.close();
        return results;
    }

    const qint64 chunkSize = 16LL * 1024 * 1024;     // 16 MB
    const qint64 overlapSize = 1024LL * 1024;        // 1 MB

    QVector<FileSignature> signatures =
        SignatureDatabase::loadDefaultSignatures();

    SignatureScanner signatureScanner;

    QVector<RecoverableFile> batch;
    batch.reserve(300);

    QByteArray previousTail;

    qint64 currentOffset = 0;
    int lastProgress = -1;

    if (onProgress)
        onProgress(0);

    while (currentOffset < totalBytes)
    {
        if (isCancelled && isCancelled())
            break;

        const qint64 remaining = totalBytes - currentOffset;
        const qint64 bytesToRead = qMin(chunkSize, remaining);

        if (!drive.seek(currentOffset))
        {
            currentOffset += bytesToRead;
            continue;
        }

        QByteArray chunk = drive.read(bytesToRead);

        if (chunk.isEmpty())
        {
            currentOffset += bytesToRead;
            continue;
        }

        QByteArray scanBuffer;

        if (!previousTail.isEmpty())
            scanBuffer = previousTail + chunk;
        else
            scanBuffer = chunk;

        const qint64 scanBaseOffset =
            currentOffset - previousTail.size();

        QVector<RecoverableFile> foundFiles =
            signatureScanner.scanBuffer(
                scanBuffer,
                scanBaseOffset,
                signatures
            );

        for (const RecoverableFile& file : foundFiles)
        {
            if (file.offset < currentOffset)
                continue;

            batch.append(file);

            if (batch.size() >= 300)
            {
                if (onBatch)
                    onBatch(batch);

                batch.clear();
            }
        }

        if (scanBuffer.size() > overlapSize)
            previousTail = scanBuffer.right(overlapSize);
        else
            previousTail = scanBuffer;

        currentOffset += chunk.size();

        int progress =
            qBound(
                0,
                static_cast<int>((currentOffset * 100) / totalBytes),
                100
            );

        if (progress != lastProgress)
        {
            lastProgress = progress;

            if (onProgress)
                onProgress(progress);
        }
    }

    if (!batch.isEmpty() && onBatch)
    {
        onBatch(batch);
        batch.clear();
    }

    if (onProgress && !(isCancelled && isCancelled()))
        onProgress(100);

    drive.close();

    return results;
}