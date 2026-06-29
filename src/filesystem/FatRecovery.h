#pragma once

#include <QString>
#include <QByteArray>
#include <QVector>
#include <QSet>
#include <functional>

#include "models/RecoverableFile.h"

class QFile;

/*
 * Recuperación de archivos basada en la tabla del sistema de archivos FAT
 * (FAT12 / FAT16 / FAT32). A diferencia del carving por firmas, esto lee las
 * entradas de directorio del volumen — incluidas las marcadas como borradas
 * (primer byte 0xE5) — y reconstruye cada archivo con su nombre, ruta, fecha y
 * tamaño originales, igual que hace Tenorshare 4DDiG en su sección "Reconstruido".
 */
class FatRecovery
{
public:
    using ProgressCallback = std::function<void(int)>;
    using BatchCallback    = std::function<void(const QVector<RecoverableFile>&)>;
    using CancelCallback   = std::function<bool()>;

    // Detecta si el primer sector corresponde a un volumen FAT.
    static bool isFatBootSector(const QByteArray& bootSector);

    QVector<RecoverableFile> recover(
        const QString& devicePath,
        ProgressCallback onProgress = nullptr,
        BatchCallback onBatch = nullptr,
        CancelCallback isCancelled = nullptr,
        QString* errorMessage = nullptr
    );

private:
    // Parámetros del BPB (BIOS Parameter Block)
    quint16 m_bytesPerSector   = 512;
    quint8  m_sectorsPerCluster = 1;
    quint16 m_reservedSectors  = 0;
    quint8  m_numFats          = 2;
    quint16 m_rootEntCount     = 0;
    quint32 m_fatSize          = 0;
    quint32 m_rootCluster      = 2;
    quint32 m_totalSectors     = 0;

    quint32 m_firstDataSector  = 0;
    quint32 m_rootDirSectors   = 0;
    quint32 m_countOfClusters  = 0;
    bool    m_isFat32          = false;

    QFile*  m_device = nullptr;

    BatchCallback  m_onBatch;
    CancelCallback m_isCancelled;

    QVector<RecoverableFile> m_results;
    QVector<RecoverableFile> m_pendingBatch;

    int m_dirsProcessed = 0;

    // Clusters iniciales ya recuperados (para deduplicar entre el árbol vivo
    // y el escaneo de directorios huérfanos).
    QSet<quint32> m_seenStartClusters;

    qint64 clusterToByteOffset(quint32 cluster) const;
    QByteArray readSectors(quint64 startSector, quint32 count);
    QByteArray readClusterChainData(quint32 startCluster, int maxClusters);

    // Lectura alineada a sector: Windows exige que las lecturas de un volumen
    // crudo (\\.\D:) usen offset y tamaño múltiplos del sector. Este helper
    // alinea por debajo, lee de más y recorta la porción pedida.
    QByteArray readAligned(qint64 offset, qint64 length);

    quint32 nextCluster(const QByteArray& fat, quint32 cluster) const;

    void parseBpb(const QByteArray& boot);

    void parseDirectory(
        const QByteArray& fat,
        quint32 startCluster,
        const QString& path,
        int depth
    );

    void parseDirectoryEntries(
        const QByteArray& fat,
        const QByteArray& data,
        const QString& path,
        int depth
    );

    // Recorre toda el área de datos del volumen: (A) busca entradas de directorio
    // huérfanas (carpetas borradas/formateadas) y (B) detecta firmas de archivo
    // al inicio de cada cluster (carving) para reconstruir archivos cuya entrada
    // de directorio ya no existe — como hace la sección "Reconstruido" de 4DDiG.
    void scanDataArea(ProgressCallback onProgress);

    // ¿La entrada de 32 bytes en 'base' parece un archivo recuperable válido?
    bool looksLikeFileEntry(const QByteArray& data, int base) const;

    void addFileEntry(
        const QByteArray& data,
        int base,
        const QString& lfnName,
        const QString& path
    );

    // Carving: candidato detectado por firma al inicio de un cluster.
    struct CarveCandidate
    {
        qint64  offset;
        quint32 startCluster;
        QString ext;
    };

    QVector<CarveCandidate> m_carveCandidates;

    // Detecta una firma de archivo al inicio de un cluster (offset 'off' del
    // bloque). Devuelve la extensión o cadena vacía si no hay coincidencia.
    QString detectSignature(const QByteArray& block, int off) const;

    // Procesa un candidato de carving: calcula su tamaño real leyendo su
    // estructura interna y emite el RecoverableFile (en tiempo real, durante
    // el escaneo, para que los archivos aparezcan conforme se detectan).
    void emitCarvedFile(const CarveCandidate& cand);

    qint64 determineMp4Size(qint64 offset, qint64 cap);
    qint64 determineSizeByFooter(
        qint64 offset,
        const QByteArray& footer,
        qint64 cap
    );

    void flushBatch(bool force);
};
