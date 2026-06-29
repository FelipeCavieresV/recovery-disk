#include "PreviewService.h"

#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QFileInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#include <winioctl.h>
#endif

bool PreviewService::isImage(const QString& extension) const
{
    QString ext = extension.toLower();

    return ext == "jpg" ||
           ext == "jpeg" ||
           ext == "png" ||
           ext == "bmp" ||
           ext == "gif" ||
           ext == "webp" ||
           ext == "tif" ||
           ext == "tiff";
}

bool PreviewService::isVideo(const QString& extension) const
{
    QString ext = extension.toLower();

    return ext == "mp4" ||
           ext == "m4v" ||
           ext == "mov" ||
           ext == "avi" ||
           ext == "mkv" ||
           ext == "wmv" ||
           ext == "webm" ||
           ext == "flv" ||
           ext == "mpg" ||
           ext == "mpeg" ||
           ext == "3gp" ||
           ext == "swf";
}

QPixmap PreviewService::createImagePreview(
    const QString& filePath,
    const QSize& size
) const
{
    QPixmap pixmap(filePath);

    if (pixmap.isNull())
        return {};

    return pixmap.scaled(
        size,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );
}

QString PreviewService::normalizeDevicePath(
    const QString& driveRoot
) const
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

QString PreviewService::createTemporaryPreviewFile(
    const RecoverableFile& file,
    const QString& driveRoot,
    qint64 maxBytes
) const
{
    if (file.offset < 0)
        return "";

    const QString devicePath = normalizeDevicePath(driveRoot);

    QFile drive(devicePath);

    if (!drive.open(QIODevice::ReadOnly))
        return "";

    if (!drive.seek(file.offset))
    {
        drive.close();
        return "";
    }

    const QString ext = file.extension.toLower();

    QByteArray data;

    // Archivos reconstruidos desde la tabla del sistema de archivos: conocemos
    // el offset y el tamaño exactos. Leemos alineado a sector (Windows exige
    // lecturas de volumen crudo con tamaño múltiplo del sector) y recortamos.
    // 'maxBytes' (si > 0) limita la lectura — útil para miniaturas de video,
    // donde basta el inicio del archivo para extraer un fotograma.
    if (file.reconstructed && file.size > 0)
    {
        const qint64 S = 512;
        qint64 want = qMin<qint64>(file.size, 300LL * 1024 * 1024);

        if (maxBytes > 0)
            want = qMin(want, maxBytes);
        const qint64 toRead = ((want + S - 1) / S) * S;

        data = drive.read(toRead);

        if (data.size() > want)
            data.truncate(static_cast<int>(want));
    }
    else if (
        ext == "jpg" ||
        ext == "jpeg"
    )
    {
        const QByteArray footer = QByteArray::fromHex("FFD9");
        const qint64 maxBytes = 120LL * 1024 * 1024;

        QByteArray buffer;
        qint64 totalRead = 0;

        while (totalRead < maxBytes)
        {
            QByteArray chunk = drive.read(4LL * 1024 * 1024);

            if (chunk.isEmpty())
                break;

            buffer += chunk;

            const int footerIndex = buffer.indexOf(footer);

            if (footerIndex >= 0)
            {
                data = buffer.left(footerIndex + footer.size());
                break;
            }

            totalRead += chunk.size();

            if (buffer.size() > 8LL * 1024 * 1024)
            {
                QByteArray tail = buffer.right(footer.size());
                data += buffer.left(buffer.size() - footer.size());
                buffer = tail;
            }
        }

        if (data.isEmpty() && !buffer.isEmpty())
            data = buffer;
    }
    else if (ext == "png")
    {
        const QByteArray footer =
            QByteArray::fromHex("49454E44AE426082");

        const qint64 maxBytes = 80LL * 1024 * 1024;

        QByteArray buffer;
        qint64 totalRead = 0;

        while (totalRead < maxBytes)
        {
            QByteArray chunk = drive.read(4LL * 1024 * 1024);

            if (chunk.isEmpty())
                break;

            buffer += chunk;

            const int footerIndex = buffer.indexOf(footer);

            if (footerIndex >= 0)
            {
                data = buffer.left(footerIndex + footer.size());
                break;
            }

            totalRead += chunk.size();
        }

        if (data.isEmpty() && !buffer.isEmpty())
            data = buffer;
    }
    else if (isVideo(ext))
    {
        const qint64 maxPreviewBytes =
            qMin<qint64>(
                file.size > 0 ? file.size : 300LL * 1024 * 1024,
                300LL * 1024 * 1024
            );

        data = drive.read(maxPreviewBytes);
    }

    drive.close();

    if (data.isEmpty())
        return "";

    QString tempDir =
        QStandardPaths::writableLocation(
            QStandardPaths::TempLocation
        ) + "/DemogoRecoveryPreview";

    QDir().mkpath(tempDir);

    QString tempPath =
        tempDir + "/" +
        QString("preview_%1.%2")
            .arg(file.offset)
            .arg(ext);

    QFile output(tempPath);

    if (!output.open(QIODevice::WriteOnly))
        return "";

    output.write(data);
    output.close();

    return tempPath;
}

QString PreviewService::createVideoPreviewFile(
    const RecoverableFile& file,
    const QString& driveRoot
) const
{
    if (file.offset < 0 || file.size <= 0)
        return "";

    const QString devicePath = normalizeDevicePath(driveRoot);

    QFile drive(devicePath);

    if (!drive.open(QIODevice::ReadOnly))
        return "";

    const qint64 S = 512;
    const qint64 size = file.size;

    // Inicio del archivo (ftyp + primeros fotogramas del mdat).
    const qint64 headWant = qMin<qint64>(size, 24LL * 1024 * 1024);
    const qint64 headRead = ((headWant + S - 1) / S) * S;

    if (!drive.seek(file.offset))
    {
        drive.close();
        return "";
    }

    QByteArray head = drive.read(headRead);

    if (head.size() > headWant)
        head.truncate(static_cast<int>(headWant));

    // Final del archivo (donde suele estar el índice 'moov' de los MP4/MOV).
    QByteArray tail;
    qint64 tailOffsetInFile = 0;

    if (size > headWant)
    {
        const qint64 tailWant =
            qMin<qint64>(size - headWant, 24LL * 1024 * 1024);

        const qint64 tailStart = file.offset + size - tailWant;
        const qint64 tailStartAligned = (tailStart / S) * S;
        const qint64 delta = tailStart - tailStartAligned;
        const qint64 tailRead = ((tailWant + delta + S - 1) / S) * S;

        if (drive.seek(tailStartAligned))
        {
            QByteArray raw = drive.read(tailRead);

            if (raw.size() > delta)
            {
                tail = raw.mid(
                    static_cast<int>(delta),
                    static_cast<int>(
                        qMin<qint64>(tailWant, raw.size() - delta)
                    )
                );
                tailOffsetInFile = size - tail.size();
            }
        }
    }

    drive.close();

    if (head.isEmpty())
        return "";

    QString tempDir =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
        "/DemogoRecoveryPreview";

    QDir().mkpath(tempDir);

    const QString tempPath =
        tempDir + "/" +
        QString("vthumb_%1.%2")
            .arg(file.offset)
            .arg(file.extension.toLower());

#ifdef Q_OS_WIN
    // Archivo sparse: solo el inicio y el final ocupan espacio en disco; el
    // hueco intermedio se lee como ceros sin escribirlo físicamente.
    HANDLE h = CreateFileW(
        reinterpret_cast<LPCWSTR>(tempPath.utf16()),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (h == INVALID_HANDLE_VALUE)
        return "";

    DWORD br = 0;
    DeviceIoControl(h, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &br, nullptr);

    LARGE_INTEGER li;
    li.QuadPart = size;
    SetFilePointerEx(h, li, nullptr, FILE_BEGIN);
    SetEndOfFile(h);

    LARGE_INTEGER zero;
    zero.QuadPart = 0;
    SetFilePointerEx(h, zero, nullptr, FILE_BEGIN);

    DWORD written = 0;
    WriteFile(h, head.constData(), static_cast<DWORD>(head.size()), &written, nullptr);

    if (!tail.isEmpty())
    {
        LARGE_INTEGER tp;
        tp.QuadPart = tailOffsetInFile;
        SetFilePointerEx(h, tp, nullptr, FILE_BEGIN);
        WriteFile(h, tail.constData(), static_cast<DWORD>(tail.size()), &written, nullptr);
    }

    CloseHandle(h);
    return tempPath;
#else
    QFile output(tempPath);

    if (!output.open(QIODevice::WriteOnly))
        return "";

    output.resize(size);
    output.seek(0);
    output.write(head);

    if (!tail.isEmpty())
    {
        output.seek(tailOffsetInFile);
        output.write(tail);
    }

    output.close();
    return tempPath;
#endif
}