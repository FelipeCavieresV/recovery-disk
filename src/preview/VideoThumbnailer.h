#pragma once

#include <QObject>
#include <QQueue>
#include <QImage>
#include <QString>

#include "models/RecoverableFile.h"

class QMediaPlayer;
class QVideoSink;
class QAudioOutput;
class QTimer;

/*
 * Genera miniaturas (un fotograma) de videos de forma asíncrona y SIN bloquear
 * la interfaz:
 *   1) Lee el contenido del video desde el disco crudo en un hilo de fondo
 *      (QtConcurrent) y lo guarda en un archivo temporal.
 *   2) En el hilo principal, lo decodifica con QMediaPlayer + QVideoSink
 *      (backend FFmpeg de Qt) y captura el primer fotograma válido.
 * Procesa la cola de a uno para no saturar el disco ni la memoria.
 */
class VideoThumbnailer : public QObject
{
    Q_OBJECT

public:
    explicit VideoThumbnailer(QObject* parent = nullptr);
    ~VideoThumbnailer();

    // Encola un video. 'key' identifica el archivo (índice) para la señal.
    void enqueue(
        int key,
        const RecoverableFile& file,
        const QString& driveRoot
    );

    void clearQueue();

signals:
    void thumbnailReady(int key, const QImage& image);

private:
    struct Job
    {
        int key;
        RecoverableFile file;
        QString driveRoot;
    };

    void processNext();
    void startDecoding(const QString& tempPath);
    void finishCurrent();

    QMediaPlayer* m_player = nullptr;
    QVideoSink*   m_sink = nullptr;
    QAudioOutput* m_audio = nullptr;
    QTimer*       m_timeout = nullptr;

    QQueue<Job> m_queue;

    int  m_currentKey = -1;
    bool m_busy = false;
    bool m_captured = false;
    bool m_seeked = false;
    bool m_readyToCapture = false;  // ignora fotogramas residuales del sink
};
