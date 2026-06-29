#include "FatRecovery.h"

#include <QFile>
#include <QFileInfo>
#include <QDateTime>

namespace
{
    quint16 readU16(const QByteArray& data, int offset)
    {
        if (offset + 2 > data.size())
            return 0;

        const quint8 b0 = static_cast<quint8>(data[offset]);
        const quint8 b1 = static_cast<quint8>(data[offset + 1]);

        return static_cast<quint16>(b0 | (b1 << 8));
    }

    quint32 readU32(const QByteArray& data, int offset)
    {
        if (offset + 4 > data.size())
            return 0;

        const quint8 b0 = static_cast<quint8>(data[offset]);
        const quint8 b1 = static_cast<quint8>(data[offset + 1]);
        const quint8 b2 = static_cast<quint8>(data[offset + 2]);
        const quint8 b3 = static_cast<quint8>(data[offset + 3]);

        return static_cast<quint32>(b0) |
               (static_cast<quint32>(b1) << 8) |
               (static_cast<quint32>(b2) << 16) |
               (static_cast<quint32>(b3) << 24);
    }

    QString extensionOf(const QString& name)
    {
        const int dot = name.lastIndexOf('.');

        if (dot < 0 || dot == name.size() - 1)
            return QString();

        return name.mid(dot + 1).toLower();
    }

    // Extensiones reconocidas: filtran falsos positivos en el escaneo huérfano.
    bool isKnownExtension(const QString& ext)
    {
        static const QSet<QString> known = {
            // imágenes
            "jpg", "jpeg", "png", "gif", "bmp", "tif", "tiff", "webp",
            "heic", "heif", "raw", "cr2", "nef", "arw", "dng", "psd", "ico",
            // videos
            "mp4", "m4v", "mov", "avi", "mkv", "wmv", "flv", "webm",
            "mpeg", "mpg", "3gp", "swf", "ts", "vob", "m2ts",
            // audio
            "mp3", "wav", "flac", "ogg", "m4a", "wma", "aac", "aiff", "mid",
            // documentos
            "pdf", "doc", "docx", "xls", "xlsx", "ppt", "pptx", "txt",
            "csv", "rtf", "odt", "ods", "odp", "xml", "json", "html", "epub",
            // comprimidos
            "zip", "rar", "7z", "tar", "gz", "bz2", "xz",
            // otros comunes
            "exe", "iso", "sql", "db", "sqlite"
        };

        return known.contains(ext.toLower());
    }

    // ¿El carácter pertenece a un nombre 8.3 válido?
    bool isValidShortNameChar(quint8 c)
    {
        if (c < 0x20)
            return false;

        // caracteres prohibidos en nombres 8.3
        static const char* forbidden = "\"*+,./:;<=>?[\\]|";

        for (const char* f = forbidden; *f; ++f)
            if (c == static_cast<quint8>(*f))
                return false;

        return true;
    }

    // Convierte fecha/hora FAT (campos de 16 bits) a QDateTime
    QDateTime fatDateTime(quint16 date, quint16 time)
    {
        if (date == 0)
            return QDateTime::currentDateTime();

        const int day   = date & 0x1F;
        const int month = (date >> 5) & 0x0F;
        const int year  = 1980 + ((date >> 9) & 0x7F);

        const int sec  = (time & 0x1F) * 2;
        const int min  = (time >> 5) & 0x3F;
        const int hour = (time >> 11) & 0x1F;

        QDate d(year, qBound(1, month, 12), qBound(1, day, 31));
        QTime t(qBound(0, hour, 23), qBound(0, min, 59), qBound(0, sec, 59));

        if (!d.isValid())
            return QDateTime::currentDateTime();

        return QDateTime(d, t);
    }
}

bool FatRecovery::isFatBootSector(const QByteArray& bootSector)
{
    if (bootSector.size() < 512)
        return false;

    // Firma 0x55AA al final del sector de arranque
    const quint8 sig0 = static_cast<quint8>(bootSector[510]);
    const quint8 sig1 = static_cast<quint8>(bootSector[511]);

    if (sig0 != 0x55 || sig1 != 0xAA)
        return false;

    const quint16 bytesPerSector = readU16(bootSector, 0x0B);

    if (bytesPerSector != 512 && bytesPerSector != 1024 &&
        bytesPerSector != 2048 && bytesPerSector != 4096)
        return false;

    const quint8 sectorsPerCluster = static_cast<quint8>(bootSector[0x0D]);

    if (sectorsPerCluster == 0)
        return false;

    // Cadena identificadora "FAT" en las dos posiciones habituales
    const QByteArray id16 = bootSector.mid(0x36, 5);  // FAT12/FAT16
    const QByteArray id32 = bootSector.mid(0x52, 5);  // FAT32

    if (id16.startsWith("FAT") || id32.startsWith("FAT"))
        return true;

    // Fallback: NTFS empieza con "NTFS" en 0x03; si no es NTFS y el BPB
    // es coherente, lo tratamos como FAT.
    const QByteArray oem = bootSector.mid(0x03, 4);

    if (oem == "NTFS" || oem == "EXFAT")
        return false;

    return false;
}

void FatRecovery::parseBpb(const QByteArray& boot)
{
    m_bytesPerSector    = readU16(boot, 0x0B);
    m_sectorsPerCluster = static_cast<quint8>(boot[0x0D]);
    m_reservedSectors   = readU16(boot, 0x0E);
    m_numFats           = static_cast<quint8>(boot[0x10]);
    m_rootEntCount      = readU16(boot, 0x11);

    const quint16 totalSectors16 = readU16(boot, 0x13);
    const quint16 fatSize16      = readU16(boot, 0x16);
    const quint32 totalSectors32 = readU32(boot, 0x20);
    const quint32 fatSize32      = readU32(boot, 0x24);

    m_totalSectors = (totalSectors16 != 0) ? totalSectors16 : totalSectors32;
    m_fatSize      = (fatSize16 != 0) ? fatSize16 : fatSize32;
    m_rootCluster  = readU32(boot, 0x2C);

    if (m_bytesPerSector == 0)
        m_bytesPerSector = 512;

    // Región del directorio raíz (solo FAT12/16; en FAT32 es 0)
    m_rootDirSectors =
        ((m_rootEntCount * 32) + (m_bytesPerSector - 1)) / m_bytesPerSector;

    m_firstDataSector =
        m_reservedSectors +
        (m_numFats * m_fatSize) +
        m_rootDirSectors;

    const quint32 dataSectors =
        m_totalSectors - m_firstDataSector;

    m_countOfClusters =
        (m_sectorsPerCluster > 0)
            ? dataSectors / m_sectorsPerCluster
            : 0;

    // Determinación oficial del tipo de FAT por número de clusters
    m_isFat32 = (m_rootEntCount == 0 && fatSize16 == 0);
}

qint64 FatRecovery::clusterToByteOffset(quint32 cluster) const
{
    const quint64 sector =
        m_firstDataSector +
        (static_cast<quint64>(cluster - 2) * m_sectorsPerCluster);

    return static_cast<qint64>(sector) * m_bytesPerSector;
}

QByteArray FatRecovery::readSectors(quint64 startSector, quint32 count)
{
    if (!m_device)
        return {};

    const qint64 offset =
        static_cast<qint64>(startSector) * m_bytesPerSector;

    const qint64 length =
        static_cast<qint64>(count) * m_bytesPerSector;

    if (!m_device->seek(offset))
        return {};

    return m_device->read(length);
}

QByteArray FatRecovery::readAligned(qint64 offset, qint64 length)
{
    if (!m_device || length <= 0 || offset < 0)
        return {};

    const qint64 S = (m_bytesPerSector > 0) ? m_bytesPerSector : 512;

    const qint64 alignedStart = (offset / S) * S;
    const qint64 delta = offset - alignedStart;

    qint64 toRead = length + delta;
    toRead = ((toRead + S - 1) / S) * S;  // redondear hacia arriba a sector

    if (!m_device->seek(alignedStart))
        return {};

    QByteArray raw = m_device->read(toRead);

    if (raw.size() <= delta)
        return {};

    return raw.mid(
        static_cast<int>(delta),
        static_cast<int>(qMin<qint64>(length, raw.size() - delta))
    );
}

QByteArray FatRecovery::readClusterChainData(
    quint32 startCluster,
    int maxClusters
)
{
    QByteArray out;

    if (startCluster < 2)
        return out;

    const quint64 startSector =
        m_firstDataSector +
        (static_cast<quint64>(startCluster - 2) * m_sectorsPerCluster);

    // Para directorios borrados la cadena FAT se pierde, así que leemos de
    // forma contigua un número acotado de clusters.
    out = readSectors(
        startSector,
        static_cast<quint32>(m_sectorsPerCluster) * maxClusters
    );

    return out;
}

quint32 FatRecovery::nextCluster(const QByteArray& fat, quint32 cluster) const
{
    if (m_isFat32)
    {
        const int idx = static_cast<int>(cluster * 4);

        if (idx + 4 > fat.size())
            return 0x0FFFFFFF;

        return readU32(fat, idx) & 0x0FFFFFFF;
    }
    else
    {
        // FAT16
        const int idx = static_cast<int>(cluster * 2);

        if (idx + 2 > fat.size())
            return 0xFFFF;

        return readU16(fat, idx);
    }
}

QVector<RecoverableFile> FatRecovery::recover(
    const QString& devicePath,
    ProgressCallback onProgress,
    BatchCallback onBatch,
    CancelCallback isCancelled,
    QString* errorMessage
)
{
    m_results.clear();
    m_pendingBatch.clear();
    m_onBatch = onBatch;
    m_isCancelled = isCancelled;
    m_dirsProcessed = 0;

    QFile device(devicePath);

    if (!device.open(QIODevice::ReadOnly))
    {
        if (errorMessage)
            *errorMessage =
                "No se pudo abrir la unidad:\n" + devicePath +
                "\n\nEjecuta DemogoRecovery como administrador.";
        return m_results;
    }

    m_device = &device;

    // Leer sector de arranque
    QByteArray boot = device.read(512);

    if (!isFatBootSector(boot))
    {
        if (errorMessage)
            *errorMessage = "__NOT_FAT__";  // señal para usar otro método

        device.close();
        m_device = nullptr;
        return m_results;
    }

    parseBpb(boot);

    if (m_bytesPerSector == 0 || m_sectorsPerCluster == 0)
    {
        if (errorMessage)
            *errorMessage = "Parámetros FAT inválidos.";
        device.close();
        m_device = nullptr;
        return m_results;
    }

    if (onProgress)
        onProgress(2);

    // Cargar la primera FAT en memoria (para seguir cadenas de directorios vivos)
    QByteArray fat =
        readSectors(m_reservedSectors, m_fatSize);

    if (onProgress)
        onProgress(8);

    // Paso A: recorrer el árbol de directorios vivo (archivos existentes y
    // entradas borradas que aún cuelgan de carpetas existentes).
    if (m_isFat32)
    {
        parseDirectory(fat, m_rootCluster, QString(), 0);
    }
    else
    {
        // FAT12/16: el directorio raíz es una región fija de sectores
        const quint64 rootStartSector =
            m_reservedSectors + (m_numFats * m_fatSize);

        QByteArray rootData =
            readSectors(rootStartSector, m_rootDirSectors);

        parseDirectoryEntries(fat, rootData, QString(), 0);
    }

    flushBatch(true);

    // Paso B: recorrer TODO el volumen. Detecta tablas de directorio huérfanas
    // (carpetas borradas/formateadas) y firmas de archivo al inicio de cada
    // cluster (carving). Cada archivo detectado se reconstruye y emite en el
    // momento, así aparecen conforme avanza el escaneo (como hace 4DDiG).
    m_carveCandidates.clear();
    scanDataArea(onProgress);

    flushBatch(true);

    if (onProgress)
        onProgress(100);

    device.close();
    m_device = nullptr;

    return m_results;
}

void FatRecovery::parseDirectory(
    const QByteArray& fat,
    quint32 startCluster,
    const QString& path,
    int depth
)
{
    if (depth > 24)
        return;

    if (m_isCancelled && m_isCancelled())
        return;

    if (startCluster < 2)
        return;

    // Seguir la cadena de clusters del directorio (está vivo)
    QByteArray data;

    quint32 cluster = startCluster;
    int guard = 0;

    const quint32 clusterBytes =
        static_cast<quint32>(m_sectorsPerCluster) * m_bytesPerSector;

    while (cluster >= 2 && cluster < 0x0FFFFFF8 && guard < 100000)
    {
        const quint64 sector =
            m_firstDataSector +
            (static_cast<quint64>(cluster - 2) * m_sectorsPerCluster);

        data += readSectors(sector, m_sectorsPerCluster);

        const quint32 next = nextCluster(fat, cluster);

        if (next < 2 || next >= (m_isFat32 ? 0x0FFFFFF8u : 0xFFF8u))
            break;

        if (next == cluster)
            break;

        cluster = next;
        ++guard;

        if (data.size() > 64 * 1024 * 1024)  // tope de seguridad por directorio
            break;
    }

    Q_UNUSED(clusterBytes);

    parseDirectoryEntries(fat, data, path, depth);
}

void FatRecovery::parseDirectoryEntries(
    const QByteArray& fat,
    const QByteArray& data,
    const QString& path,
    int depth
)
{
    if (m_isCancelled && m_isCancelled())
        return;

    ++m_dirsProcessed;

    QString lfnAccum;          // nombre largo acumulado
    bool    haveLfn = false;

    QVector<QPair<quint32, QString>> subdirsToVisit;  // cluster, path

    const int entryCount = data.size() / 32;

    for (int i = 0; i < entryCount; ++i)
    {
        const int base = i * 32;

        const quint8 first = static_cast<quint8>(data[base]);

        // 0x00 -> resto del directorio sin usar. Seguimos de todos modos para
        // intentar recuperar entradas borradas que pudieran quedar más abajo.
        if (first == 0x00)
        {
            lfnAccum.clear();
            haveLfn = false;
            continue;
        }

        const quint8 attr = static_cast<quint8>(data[base + 0x0B]);

        const bool isDeleted = (first == 0xE5);

        // Entrada de nombre largo (LFN)
        if (attr == 0x0F)
        {
            // Extraer los 13 caracteres UTF-16 de esta pieza
            QString part;

            auto appendChars = [&](int off, int count)
            {
                for (int c = 0; c < count; ++c)
                {
                    const int p = base + off + c * 2;

                    if (p + 1 >= data.size())
                        break;

                    const quint16 ch = readU16(data, p);

                    if (ch == 0x0000 || ch == 0xFFFF)
                        return;

                    part.append(QChar(ch));
                }
            };

            appendChars(0x01, 5);
            appendChars(0x0E, 6);
            appendChars(0x1C, 2);

            // Las piezas LFN se almacenan en orden inverso: prependemos.
            lfnAccum.prepend(part);
            haveLfn = true;
            continue;
        }

        // Etiqueta de volumen -> ignorar
        if (attr & 0x08)
        {
            lfnAccum.clear();
            haveLfn = false;
            continue;
        }

        const bool isDir = (attr & 0x10) != 0;

        // Nombre 8.3
        QString shortName;
        {
            QByteArray raw = data.mid(base, 11);

            QString namePart =
                QString::fromLatin1(raw.left(8)).trimmed();

            QString extPart =
                QString::fromLatin1(raw.mid(8, 3)).trimmed();

            if (isDeleted && !namePart.isEmpty())
                namePart[0] = QChar('_');  // primer byte se perdió (0xE5)

            shortName = namePart;

            if (!extPart.isEmpty())
                shortName += "." + extPart;
        }

        QString finalName = haveLfn && !lfnAccum.isEmpty()
            ? lfnAccum
            : shortName;

        lfnAccum.clear();
        haveLfn = false;

        // Entradas "." y ".."
        if (shortName == "." || shortName == ".." ||
            finalName == "." || finalName == "..")
            continue;

        const quint16 clusterHigh = readU16(data, base + 0x14);
        const quint16 clusterLow  = readU16(data, base + 0x1A);

        const quint32 startCluster =
            (static_cast<quint32>(clusterHigh) << 16) | clusterLow;

        const quint32 fileSize = readU32(data, base + 0x1C);

        const quint16 wDate = readU16(data, base + 0x18);  // fecha de escritura
        const quint16 wTime = readU16(data, base + 0x16);

        if (isDir)
        {
            // Solo descendemos en directorios VIVOS (su cadena FAT es válida)
            if (!isDeleted && startCluster >= 2)
            {
                const QString childPath =
                    path.isEmpty() ? finalName : path + "/" + finalName;

                subdirsToVisit.append({ startCluster, childPath });
            }

            continue;
        }

        // Archivo regular
        if (startCluster < 2 && fileSize == 0)
            continue;  // entrada vacía sin datos

        RecoverableFile file;

        file.originalName = finalName;
        file.folderPath   = path;
        file.originalPath =
            path.isEmpty() ? finalName : path + "/" + finalName;

        file.extension     = extensionOf(finalName);
        file.size          = fileSize;
        file.deletedAt     = fatDateTime(wDate, wTime);
        file.source        = RecoverySource::DeepScan;
        file.reconstructed = true;
        file.isDeleted     = isDeleted;

        // offset físico del primer cluster (para preview/recuperación)
        file.offset =
            (startCluster >= 2) ? clusterToByteOffset(startCluster) : -1;

        file.hasValidHeader = true;
        file.hasValidFooter = (fileSize > 0);
        file.quality =
            isDeleted ? RecoveryQuality::Regular : RecoveryQuality::Good;

        if (startCluster >= 2)
            m_seenStartClusters.insert(startCluster);

        m_pendingBatch.append(file);
        m_results.append(file);

        flushBatch(false);
    }

    // Recursión en subdirectorios vivos
    for (const auto& sub : subdirsToVisit)
    {
        if (m_isCancelled && m_isCancelled())
            return;

        parseDirectory(fat, sub.first, sub.second, depth + 1);
    }
}

bool FatRecovery::looksLikeFileEntry(const QByteArray& data, int base) const
{
    if (base + 32 > data.size())
        return false;

    const quint8 first = static_cast<quint8>(data[base]);

    if (first == 0x00)
        return false;

    const quint8 attr = static_cast<quint8>(data[base + 0x0B]);

    // Descartar LFN, etiqueta de volumen, directorios y bits reservados.
    if (attr == 0x0F)
        return false;

    if (attr & 0x08)   // volume label
        return false;

    if (attr & 0x10)   // directorio
        return false;

    if (attr & 0xC0)   // bits reservados deben ser 0
        return false;

    // Validar caracteres del nombre 8.3 (saltando el primer byte si está borrado)
    const int startChar = (first == 0xE5) ? 1 : 0;

    if (first != 0xE5 && !isValidShortNameChar(first))
        return false;

    for (int i = startChar; i < 11; ++i)
    {
        const quint8 c = static_cast<quint8>(data[base + i]);

        if (c == 0x20)        // espacios de relleno permitidos
            continue;

        if (!isValidShortNameChar(c))
            return false;
    }

    // La extensión debe ser conocida (filtro fuerte contra falsos positivos)
    QByteArray rawExt = data.mid(base + 8, 3);
    QString ext = QString::fromLatin1(rawExt).trimmed().toLower();

    if (!isKnownExtension(ext))
        return false;

    // Cluster inicial y tamaño deben ser coherentes con el volumen
    const quint16 clusterHigh = readU16(data, base + 0x14);
    const quint16 clusterLow  = readU16(data, base + 0x1A);

    const quint32 startCluster =
        (static_cast<quint32>(clusterHigh) << 16) | clusterLow;

    const quint32 maxCluster = m_countOfClusters + 1;

    if (startCluster < 2 || startCluster > maxCluster)
        return false;

    const quint32 fileSize = readU32(data, base + 0x1C);

    const quint64 volumeBytes =
        static_cast<quint64>(m_totalSectors) * m_bytesPerSector;

    if (fileSize == 0 || fileSize > volumeBytes)
        return false;

    return true;
}

void FatRecovery::addFileEntry(
    const QByteArray& data,
    int base,
    const QString& lfnName,
    const QString& path
)
{
    const quint8 first = static_cast<quint8>(data[base]);
    const bool isDeleted = (first == 0xE5);

    QByteArray raw = data.mid(base, 11);

    QString namePart = QString::fromLatin1(raw.left(8)).trimmed();
    QString extPart  = QString::fromLatin1(raw.mid(8, 3)).trimmed();

    if (isDeleted && !namePart.isEmpty())
        namePart[0] = QChar('_');

    QString shortName = namePart;

    if (!extPart.isEmpty())
        shortName += "." + extPart;

    QString finalName =
        !lfnName.isEmpty() ? lfnName : shortName;

    const quint16 clusterHigh = readU16(data, base + 0x14);
    const quint16 clusterLow  = readU16(data, base + 0x1A);

    const quint32 startCluster =
        (static_cast<quint32>(clusterHigh) << 16) | clusterLow;

    if (startCluster < 2)
        return;

    // Deduplicar contra el árbol vivo y otras entradas huérfanas
    if (m_seenStartClusters.contains(startCluster))
        return;

    m_seenStartClusters.insert(startCluster);

    const quint32 fileSize = readU32(data, base + 0x1C);

    const quint16 wDate = readU16(data, base + 0x18);
    const quint16 wTime = readU16(data, base + 0x16);

    RecoverableFile file;

    file.originalName = finalName;
    file.folderPath   = path;
    file.originalPath =
        path.isEmpty() ? finalName : path + "/" + finalName;

    file.extension     = extensionOf(finalName);
    file.size          = fileSize;
    file.deletedAt     = fatDateTime(wDate, wTime);
    file.source        = RecoverySource::DeepScan;
    file.reconstructed = true;
    file.isDeleted     = true;
    file.offset        = clusterToByteOffset(startCluster);
    file.hasValidHeader = true;
    file.hasValidFooter = true;
    file.quality        = RecoveryQuality::Regular;

    m_pendingBatch.append(file);
    m_results.append(file);

    flushBatch(false);
}

QString FatRecovery::detectSignature(const QByteArray& block, int off) const
{
    auto match = [&](const char* hex) -> bool
    {
        QByteArray sig = QByteArray::fromHex(hex);

        if (off + sig.size() > block.size())
            return false;

        return block.mid(off, sig.size()) == sig;
    };

    auto matchAscii = [&](const char* s, int at) -> bool
    {
        const int len = static_cast<int>(qstrlen(s));

        if (off + at + len > block.size())
            return false;

        return block.mid(off + at, len) == QByteArray(s);
    };

    // ── Imágenes ──
    if (match("FFD8FFE0") || match("FFD8FFE1") ||
        match("FFD8FFDB") || match("FFD8FFE8") || match("FFD8FFEE"))
        return "jpg";

    if (match("89504E470D0A1A0A"))
        return "png";

    if (match("474946383761") || match("474946383961"))
        return "gif";

    if (match("49492A00") || match("4D4D002A"))
        return "tiff";

    if (match("38425053"))
        return "psd";

    // ── Vídeo: contenedor MP4/MOV (ftyp box). "ftyp" en offset +4 ──
    if (matchAscii("ftyp", 4))
    {
        // El brand (4 bytes tras "ftyp") distingue el subtipo
        const QByteArray brand = block.mid(off + 8, 4);

        if (brand.startsWith("qt"))
            return "mov";

        if (brand.startsWith("M4V") || brand.startsWith("m4v"))
            return "m4v";

        if (brand.startsWith("M4A"))
            return "m4a";

        if (brand.startsWith("3g"))
            return "3gp";

        return "mp4";
    }

    // QuickTime con 'moov'/'mdat' al inicio (algunos .mov)
    if (matchAscii("moov", 4) || matchAscii("mdat", 4))
        return "mov";

    if (match("1A45DFA3"))
        return "mkv";

    // ASF / WMV / WMA
    if (match("3026B2758E66CF11"))
        return "wmv";

    // SWF (Flash): FWS (sin comprimir), CWS (zlib), ZWS (lzma)
    if (matchAscii("FWS", 0) || matchAscii("CWS", 0) || matchAscii("ZWS", 0))
        return "swf";

    // AVI: RIFF .... AVI
    if (matchAscii("RIFF", 0) && matchAscii("AVI ", 8))
        return "avi";

    // WAV: RIFF .... WAVE
    if (matchAscii("RIFF", 0) && matchAscii("WAVE", 8))
        return "wav";

    // ── Audio ──
    if (match("494433"))           // ID3 (mp3)
        return "mp3";

    if (matchAscii("fLaC", 0))
        return "flac";

    if (matchAscii("OggS", 0))
        return "ogg";

    // ── Documentos / comprimidos ──
    if (match("25504446"))         // %PDF
        return "pdf";

    if (match("D0CF11E0A1B11AE1"))  // Office 97-2003
        return "doc";

    if (match("504B0304"))          // ZIP / docx / xlsx
        return "zip";

    if (match("526172211A0700") || match("526172211A070100"))
        return "rar";

    if (match("377ABCAF271C"))
        return "7z";

    return QString();
}

void FatRecovery::scanDataArea(ProgressCallback onProgress)
{
    if (!m_device)
        return;

    const qint64 dataStart =
        static_cast<qint64>(m_firstDataSector) * m_bytesPerSector;

    const qint64 volumeBytes =
        static_cast<qint64>(m_totalSectors) * m_bytesPerSector;

    if (volumeBytes <= dataStart)
        return;

    const qint64 dataBytes = volumeBytes - dataStart;

    const qint64 clusterBytes =
        static_cast<qint64>(m_sectorsPerCluster) * m_bytesPerSector;

    if (clusterBytes <= 0)
        return;

    // Bloques de 16 MB (múltiplo del tamaño de cluster y de 32 bytes).
    const qint64 blockSize = 16LL * 1024 * 1024;

    const QString orphanPath = "Recuperado";

    QString lfnAccum;
    bool    haveLfn = false;

    qint64 pos = dataStart;
    int lastProgress = 8;

    const int maxFiles = 200000;  // tope de seguridad

    while (pos < volumeBytes)
    {
        if (m_isCancelled && m_isCancelled())
            break;

        if (m_results.size() + m_carveCandidates.size() >= maxFiles)
            break;

        const qint64 toRead = qMin(blockSize, volumeBytes - pos);

        if (!m_device->seek(pos))
            break;

        QByteArray block = m_device->read(toRead);

        if (block.isEmpty())
            break;

        // ── (A) Escaneo de entradas de directorio huérfanas (32 bytes) ──
        const int entryCount = block.size() / 32;

        for (int i = 0; i < entryCount; ++i)
        {
            const int base = i * 32;

            const quint8 first = static_cast<quint8>(block[base]);
            const quint8 attr  = static_cast<quint8>(block[base + 0x0B]);

            if (first == 0x00)
            {
                lfnAccum.clear();
                haveLfn = false;
                continue;
            }

            if (attr == 0x0F)  // pieza LFN
            {
                QString part;

                auto appendChars = [&](int o, int count)
                {
                    for (int c = 0; c < count; ++c)
                    {
                        const int p = base + o + c * 2;

                        if (p + 1 >= block.size())
                            break;

                        const quint16 ch = readU16(block, p);

                        if (ch == 0x0000 || ch == 0xFFFF)
                            return;

                        part.append(QChar(ch));
                    }
                };

                appendChars(0x01, 5);
                appendChars(0x0E, 6);
                appendChars(0x1C, 2);

                lfnAccum.prepend(part);
                haveLfn = true;
                continue;
            }

            if (looksLikeFileEntry(block, base))
            {
                addFileEntry(
                    block,
                    base,
                    haveLfn ? lfnAccum : QString(),
                    orphanPath
                );
            }

            lfnAccum.clear();
            haveLfn = false;
        }

        // ── (B) Carving: firma al inicio de cada cluster del bloque ──
        // El archivo se reconstruye y emite en el momento. Los seeks que hace
        // emitCarvedFile no afectan al escaneo: la próxima iteración del while
        // vuelve a hacer seek(pos) sobre el siguiente bloque.
        for (qint64 off = 0; off + 16 <= block.size(); off += clusterBytes)
        {
            const QString ext = detectSignature(block, static_cast<int>(off));

            if (ext.isEmpty())
                continue;

            const qint64 absOffset = pos + off;

            const quint32 startCluster = static_cast<quint32>(
                ((absOffset - dataStart) / clusterBytes) + 2
            );

            if (m_seenStartClusters.contains(startCluster))
                continue;

            m_seenStartClusters.insert(startCluster);

            emitCarvedFile({ absOffset, startCluster, ext });
        }

        // Tras los seeks del carving, reposicionar al siguiente bloque.
        pos += block.size();

        if (onProgress)
        {
            // El escaneo del volumen ocupa el rango 8%..99%.
            const int progress =
                8 + static_cast<int>(((pos - dataStart) * 91) / dataBytes);

            const int clamped = qBound(8, progress, 99);

            if (clamped != lastProgress)
            {
                lastProgress = clamped;
                onProgress(clamped);
            }
        }
    }
}

qint64 FatRecovery::determineMp4Size(qint64 offset, qint64 cap)
{
    // Un MP4/MOV es una secuencia de "boxes": [size(4 BE)][type(4)][datos...].
    // Sumamos los tamaños de boxes válidos consecutivos para obtener el total.
    qint64 total = 0;
    qint64 p = offset;

    int boxes = 0;

    while (boxes < 100000)
    {
        QByteArray head = readAligned(p, 16);

        if (head.size() < 8)
            break;

        quint64 boxSize =
            (static_cast<quint8>(head[0]) << 24) |
            (static_cast<quint8>(head[1]) << 16) |
            (static_cast<quint8>(head[2]) << 8)  |
            (static_cast<quint8>(head[3]));

        const QByteArray type = head.mid(4, 4);

        // El tipo debe ser ASCII imprimible (letras/números/espacio)
        bool validType = true;

        for (int i = 0; i < 4; ++i)
        {
            const quint8 c = static_cast<quint8>(type[i]);

            if (c < 0x20 || c > 0x7E)
            {
                validType = false;
                break;
            }
        }

        if (!validType)
            break;

        if (boxSize == 1)
        {
            // tamaño extendido de 64 bits en los 8 bytes siguientes
            if (head.size() < 16)
                break;

            boxSize = 0;
            for (int i = 0; i < 8; ++i)
                boxSize = (boxSize << 8) | static_cast<quint8>(head[8 + i]);
        }

        if (boxSize < 8)
            break;

        total += static_cast<qint64>(boxSize);
        p += static_cast<qint64>(boxSize);
        ++boxes;

        if (total > cap)
        {
            total = cap;
            break;
        }
    }

    return total;
}

qint64 FatRecovery::determineSizeByFooter(
    qint64 offset,
    const QByteArray& footer,
    qint64 cap
)
{
    if (footer.isEmpty())
        return 0;

    const qint64 chunkSize = 4LL * 1024 * 1024;

    qint64 pos = offset;
    qint64 scanned = 0;
    QByteArray carry;

    while (scanned < cap)
    {
        QByteArray chunk = readAligned(pos, chunkSize);

        if (chunk.isEmpty())
            break;

        QByteArray buf = carry + chunk;

        const int idx = buf.indexOf(footer);

        if (idx >= 0)
        {
            const qint64 sizeBeforeCarry = scanned - carry.size();
            return sizeBeforeCarry + idx + footer.size();
        }

        // conservar una cola por si el footer cruza el límite del chunk
        carry = buf.right(footer.size());
        scanned += chunk.size();
        pos += chunk.size();
    }

    return 0;
}

void FatRecovery::emitCarvedFile(const CarveCandidate& cand)
{
    const QString ext = cand.ext;

    qint64 size = 0;

    const qint64 cap = 5LL * 1024 * 1024 * 1024;  // tope 5 GB

    if (ext == "mp4" || ext == "mov" || ext == "m4v" ||
        ext == "m4a" || ext == "3gp")
    {
        size = determineMp4Size(cand.offset, cap);
    }
    else if (ext == "jpg")
    {
        size = determineSizeByFooter(
            cand.offset, QByteArray::fromHex("FFD9"), 100LL * 1024 * 1024);
    }
    else if (ext == "png")
    {
        size = determineSizeByFooter(
            cand.offset,
            QByteArray::fromHex("49454E44AE426082"),
            100LL * 1024 * 1024);
    }
    else if (ext == "gif")
    {
        size = determineSizeByFooter(
            cand.offset, QByteArray::fromHex("003B"), 50LL * 1024 * 1024);
    }
    else if (ext == "pdf")
    {
        size = determineSizeByFooter(
            cand.offset, QByteArray("%%EOF"), 200LL * 1024 * 1024);
    }
    else if (ext == "swf")
    {
        // FWS: bytes 4-7 = longitud total (little endian)
        QByteArray head = readAligned(cand.offset, 8);

        if (head.size() >= 8 && head.startsWith("FWS"))
        {
            size = static_cast<quint8>(head[4]) |
                   (static_cast<quint8>(head[5]) << 8) |
                   (static_cast<quint8>(head[6]) << 16) |
                   (static_cast<quint32>(static_cast<quint8>(head[7])) << 24);
        }
    }

    // Si no se pudo determinar el tamaño, usar un valor por defecto.
    if (size <= 0)
    {
        const bool isVideo =
            (ext == "mp4" || ext == "mov" || ext == "m4v" ||
             ext == "avi" || ext == "mkv" || ext == "wmv" ||
             ext == "3gp" || ext == "swf");

        size = isVideo ? (64LL * 1024 * 1024) : (4LL * 1024 * 1024);
    }

    RecoverableFile file;

    file.originalName =
        QString("recuperado_%1.%2").arg(cand.startCluster).arg(ext);

    file.folderPath   = "Reconstruido";
    file.originalPath = "Reconstruido/" + file.originalName;
    file.extension    = ext;
    file.size         = size;
    file.deletedAt    = QDateTime::currentDateTime();
    file.source        = RecoverySource::DeepScan;
    file.reconstructed = true;
    file.isDeleted     = true;
    file.offset        = cand.offset;
    file.hasValidHeader = true;
    file.hasValidFooter = true;
    file.quality        = RecoveryQuality::Good;

    m_pendingBatch.append(file);
    m_results.append(file);

    // Forzar emisión inmediata: los archivos reconstruidos por carving (videos,
    // fotos) son pocos y valiosos, así que aparecen al instante en la interfaz.
    flushBatch(true);
}

void FatRecovery::flushBatch(bool force)
{
    if (m_pendingBatch.isEmpty())
        return;

    // Umbral bajo para que los archivos aparezcan en la interfaz conforme se
    // detectan (recuperación en tiempo real, como 4DDiG). Antes era 200, lo que
    // hacía que con pocos archivos no se mostraran hasta terminar el escaneo.
    if (!force && m_pendingBatch.size() < 16)
        return;

    if (m_onBatch)
        m_onBatch(m_pendingBatch);

    m_pendingBatch.clear();
}
