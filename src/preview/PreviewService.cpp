#include "PreviewService.h"

#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QFileInfo>

bool PreviewService::isImage(const QString& extension) const
{
    QString ext = extension.toLower();

    return ext == "jpg" ||
           ext == "jpeg" ||
           ext == "png" ||
           ext == "bmp" ||
           ext == "gif" ||
           ext == "webp";
}

bool PreviewService::isVideo(const QString& extension) const
{
    QString ext = extension.toLower();

    return ext == "mp4" ||
           ext == "mov" ||
           ext == "avi" ||
           ext == "mkv" ||
           ext == "wmv" ||
           ext == "webm";
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
    const QString& driveRoot
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

    if (
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
        ) + "/RecoveryDiskPreview";

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