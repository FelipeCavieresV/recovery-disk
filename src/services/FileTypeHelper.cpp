#include "FileTypeHelper.h"

#include <QSet>
#include <QFileInfo>

QString FileTypeHelper::typeKey(const QString& extension)
{
    QString ext = extension.toLower().trimmed();

    static const QSet<QString> images = {
        "jpg", "jpeg", "png", "bmp", "gif", "webp", "tif", "tiff", "heic"
    };

    static const QSet<QString> videos = {
        "mp4", "mov", "avi", "mkv", "wmv", "flv", "webm", "mpeg", "mpg"
    };

    static const QSet<QString> documents = {
        "pdf", "doc", "docx", "xls", "xlsx", "ppt", "pptx",
        "txt", "csv", "rtf", "odt", "ods", "odp"
    };

    static const QSet<QString> audio = {
        "mp3", "wav", "flac", "aac", "ogg", "m4a", "wma"
    };

    static const QSet<QString> compressed = {
        "zip", "rar", "7z", "tar", "gz", "bz2", "xz"
    };

    if (images.contains(ext))
        return "image";

    if (videos.contains(ext))
        return "video";

    if (documents.contains(ext))
        return "document";

    if (audio.contains(ext))
        return "audio";

    if (compressed.contains(ext))
        return "compressed";

    return "other";
}

QString FileTypeHelper::typeName(const QString& extension)
{
    return categoryTitle(typeKey(extension));
}

QStringList FileTypeHelper::categoryKeys()
{
    return {
        "all",
        "image",
        "video",
        "document",
        "audio",
        "compressed",
        "other"
    };
}

QString FileTypeHelper::categoryTitle(const QString& key)
{
    if (key == "all")
        return "Todos";

    if (key == "image")
        return "Imágenes";

    if (key == "video")
        return "Videos";

    if (key == "document")
        return "Documentos";

    if (key == "audio")
        return "Audio";

    if (key == "compressed")
        return "Comprimidos";

    return "Otros";
}

RecoveryQuality FileTypeHelper::recoveryQuality(
    const RecoverableFile& file
)
{
    if (file.currentPath.isEmpty())
        return RecoveryQuality::Bad;

    QFileInfo info(file.currentPath);

    if (!info.exists() || !info.isFile())
        return RecoveryQuality::Bad;

    if (file.size <= 0)
        return RecoveryQuality::Bad;

    if (file.size < 10 * 1024)
        return RecoveryQuality::Regular;

    return RecoveryQuality::Good;
}

QString FileTypeHelper::recoveryQualityText(
    const RecoverableFile& file
)
{
    RecoveryQuality quality = recoveryQuality(file);

    switch (quality)
    {
        case RecoveryQuality::Good:
            return "Bueno";

        case RecoveryQuality::Regular:
            return "Regular";

        case RecoveryQuality::Bad:
            return "Malo";
    }

    return "Desconocido";
}