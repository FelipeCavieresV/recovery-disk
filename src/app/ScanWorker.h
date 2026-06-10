#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <atomic>

#include "models/RecoverableFile.h"

class ScanWorker : public QObject
{
    Q_OBJECT

public:
    explicit ScanWorker(
        const QString& driveLetter,
        QObject* parent = nullptr
    );

public slots:
    void start();
    void cancel();

signals:
    void progressChanged(int progress);

    void filesFoundBatch(
        const QVector<RecoverableFile>& files
    );

    void finished();

    void failed(
        const QString& error
    );

    void cancelled();

private:
    QString m_driveLetter;

    std::atomic_bool m_cancelled = false;
    std::atomic_int m_lastProgress = -1;
};