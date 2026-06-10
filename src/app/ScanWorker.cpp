#include "ScanWorker.h"

#include "services/DeepDiskScanner.h"

#include <QDir>
#include <QFile>
#include <QTextStream>

ScanWorker::ScanWorker(
    const QString& driveLetter,
    QObject* parent
)
    : QObject(parent),
      m_driveLetter(driveLetter)
{
}

void ScanWorker::cancel()
{
    m_cancelled.store(true);
}

void ScanWorker::start()
{
    m_cancelled.store(false);
    m_lastProgress.store(-1);

    QString logPath =
        QDir::homePath() + "/Desktop/scan_worker_log.txt";

    QFile logFile(logPath);
    logFile.open(QIODevice::WriteOnly | QIODevice::Text);

    QTextStream log(&logFile);

    log << "========== ScanWorker START ==========\n";
    log << "Drive received: " << m_driveLetter << "\n";
    log.flush();

    if (m_cancelled.load())
    {
        log << "Scan cancelled before start\n";
        log.flush();
        logFile.close();

        emit cancelled();
        return;
    }

    DeepDiskScanner scanner;

    QString errorMessage;

    scanner.scanDrive(
        m_driveLetter,

        [this, &log](int progress)
        {
            if (m_cancelled.load())
                return;

            int last = m_lastProgress.load();

            if (progress == last)
                return;

            m_lastProgress.store(progress);

            emit progressChanged(progress);
        },

        [this, &log](const QVector<RecoverableFile>& batch)
        {
            if (m_cancelled.load())
                return;

            if (batch.isEmpty())
                return;


            emit filesFoundBatch(batch);
        },

        [this]()
        {
            return m_cancelled.load();
        },

        &errorMessage
    );

    log << "scanner.scanDrive finished\n";
    log << "Error message: " << errorMessage << "\n";
    log.flush();

    if (!errorMessage.isEmpty() && !m_cancelled.load())
    {
        logFile.close();
        emit failed(errorMessage);
        return;
    }

    if (m_cancelled.load())
    {
        logFile.close();
        emit cancelled();
        return;
    }

    emit progressChanged(100);

    log << "Emitting finished\n";
    log.flush();
    logFile.close();

    emit finished();
}