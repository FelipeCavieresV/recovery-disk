#include "VideoThumbnailer.h"

#include "preview/PreviewService.h"

#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
#include <QAudioOutput>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent>
#include <QFutureWatcher>

VideoThumbnailer::VideoThumbnailer(QObject* parent)
    : QObject(parent)
{
    m_player = new QMediaPlayer(this);
    m_sink   = new QVideoSink(this);
    m_audio  = new QAudioOutput(this);

    m_audio->setMuted(true);
    m_player->setAudioOutput(m_audio);
    m_player->setVideoSink(m_sink);

    m_timeout = new QTimer(this);
    m_timeout->setSingleShot(true);
    m_timeout->setInterval(8000);

    connect(
        m_sink, &QVideoSink::videoFrameChanged,
        this, [this](const QVideoFrame& frame)
        {
            // Ignorar fotogramas residuales (del video anterior) hasta que el
            // video ACTUAL esté cargado y reproduciéndose.
            if (m_captured || !m_busy || !m_readyToCapture)
                return;

            if (!frame.isValid())
                return;

            QImage image = frame.toImage();

            if (image.isNull())
                return;

            m_captured = true;
            emit thumbnailReady(m_currentKey, image);
            finishCurrent();
        }
    );

    connect(
        m_player, &QMediaPlayer::mediaStatusChanged,
        this, [this](QMediaPlayer::MediaStatus status)
        {
            if (!m_busy)
                return;

            if (status == QMediaPlayer::LoadedMedia ||
                status == QMediaPlayer::BufferedMedia)
            {
                if (!m_seeked)
                {
                    m_seeked = true;

                    const qint64 dur = m_player->duration();
                    if (dur > 2000)
                        m_player->setPosition(1000);
                }

                // El video actual ya está cargado: a partir de aquí los
                // fotogramas que lleguen son suyos (no residuales).
                m_readyToCapture = true;
                m_player->play();
            }
            else if (status == QMediaPlayer::InvalidMedia ||
                     status == QMediaPlayer::NoMedia ||
                     status == QMediaPlayer::EndOfMedia)
            {
                if (!m_captured)
                    finishCurrent();
            }
        }
    );

    connect(
        m_player, &QMediaPlayer::errorOccurred,
        this, [this](QMediaPlayer::Error, const QString&)
        {
            if (m_busy && !m_captured)
                finishCurrent();
        }
    );

    connect(
        m_timeout, &QTimer::timeout,
        this, [this]()
        {
            if (m_busy && !m_captured)
                finishCurrent();
        }
    );
}

VideoThumbnailer::~VideoThumbnailer()
{
    if (m_player)
        m_player->stop();
}

void VideoThumbnailer::enqueue(
    int key,
    const RecoverableFile& file,
    const QString& driveRoot
)
{
    m_queue.enqueue({ key, file, driveRoot });
    processNext();
}

void VideoThumbnailer::clearQueue()
{
    m_queue.clear();
}

void VideoThumbnailer::processNext()
{
    if (m_busy || m_queue.isEmpty())
        return;

    const Job job = m_queue.dequeue();

    m_currentKey = job.key;
    m_captured = false;
    m_seeked = false;
    m_readyToCapture = false;
    m_busy = true;

    // Crear el archivo temporal del video en un hilo de fondo (lectura de
    // disco), para no bloquear la interfaz. Para la miniatura basta el inicio
    // del archivo: si el índice 'moov' está al principio (faststart), se
    // decodifica un fotograma rápido. Priorizamos velocidad sobre cobertura.
    auto* watcher = new QFutureWatcher<QString>(this);

    connect(
        watcher, &QFutureWatcher<QString>::finished,
        this, [this, watcher]()
        {
            const QString tempPath = watcher->result();
            watcher->deleteLater();

            if (tempPath.isEmpty())
            {
                finishCurrent();
                return;
            }

            startDecoding(tempPath);
        }
    );

    const RecoverableFile file = job.file;
    const QString driveRoot = job.driveRoot;

    QFuture<QString> future = QtConcurrent::run(
        [file, driveRoot]() -> QString
        {
            PreviewService preview;

            // Lee solo inicio + final del video (rápido) y arma un archivo
            // sparse del tamaño correcto, para decodificar un fotograma aunque
            // el índice 'moov' esté al final.
            return preview.createVideoPreviewFile(file, driveRoot);
        }
    );

    watcher->setFuture(future);
}

void VideoThumbnailer::startDecoding(const QString& tempPath)
{
    m_seeked = false;
    m_captured = false;
    m_readyToCapture = false;   // se activará al cargar ESTE video

    m_player->setSource(QUrl::fromLocalFile(tempPath));
    m_timeout->start();
}

void VideoThumbnailer::finishCurrent()
{
    m_timeout->stop();
    m_player->stop();
    m_player->setSource(QUrl());

    // Limpiar el fotograma retenido por el sink para que no se reutilice en el
    // siguiente video.
    if (m_sink)
        m_sink->setVideoFrame(QVideoFrame());

    m_busy = false;
    m_captured = false;
    m_seeked = false;
    m_readyToCapture = false;
    m_currentKey = -1;

    processNext();
}
