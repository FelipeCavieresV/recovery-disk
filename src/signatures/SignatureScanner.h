#pragma once

#include <QByteArray>
#include <QVector>

#include "models/RecoverableFile.h"
#include "FileSignature.h"

class SignatureScanner
{
public:
    QVector<RecoverableFile> scanBuffer(
        const QByteArray& buffer,
        qint64 absoluteOffset,
        const QVector<FileSignature>& signatures
    );

private:
    int findFooter(
        const QByteArray& buffer,
        int start,
        const QByteArray& footer
    );
};