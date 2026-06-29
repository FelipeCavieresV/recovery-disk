#include "SignatureScanner.h"

#include <QDateTime>

int SignatureScanner::findFooter(
    const QByteArray& buffer,
    int start,
    const QByteArray& footer
)
{
    if (footer.isEmpty())
        return -1;

    return buffer.indexOf(
        footer,
        start
    );
}

QVector<RecoverableFile> SignatureScanner::scanBuffer(
    const QByteArray& buffer,
    qint64 absoluteOffset,
    const QVector<FileSignature>& signatures
)
{
    QVector<RecoverableFile> results;

    if (buffer.isEmpty() || signatures.isEmpty())
        return results;

    for (const FileSignature& signature : signatures)
    {
        if (signature.header.isEmpty())
            continue;

        int position = 0;

        while (position < buffer.size())
        {
            position =
                buffer.indexOf(
                    signature.header,
                    position
                );

            if (position < 0)
                break;

            // Verificar subHeader si está definido
            if (!signature.subHeader.isEmpty())
            {
                int subPos = position + signature.subHeaderOffset;

                if (
                    subPos + signature.subHeader.size() > buffer.size() ||
                    buffer.mid(subPos, signature.subHeader.size()) != signature.subHeader
                )
                {
                    position += qMax(1, signature.header.size());
                    continue;
                }
            }

            qint64 fileOffset =
                absoluteOffset + position;

            RecoverableFile file;

            file.offset = fileOffset;
            file.hasValidHeader = true;
            file.hasValidFooter = false;
            file.quality = RecoveryQuality::Unknown;

            file.originalName =
                QString("deep_%1_%2.%3")
                    .arg(signature.extension)
                    .arg(fileOffset)
                    .arg(signature.extension);

            file.originalPath =
                QString("DeepScan Offset %1")
                    .arg(fileOffset);

            file.currentPath = "";
            file.extension = signature.extension;
            file.size = 0;
            file.deletedAt = QDateTime::currentDateTime();
            file.source = RecoverySource::DeepScan;

            if (signature.hasFooter)
            {
                int footerPos =
                    findFooter(
                        buffer,
                        position + signature.header.size(),
                        signature.footer
                    );

                if (footerPos < 0)
                {
                    file.size =
                        qint64(signature.maxSizeMb) *
                        1024 *
                        1024;

                    file.hasValidFooter = false;
                    file.quality = RecoveryQuality::Regular;
                }
                else
                {
                    file.size =
                        footerPos -
                        position +
                        signature.footer.size();

                    if (file.size <= 0)
                    {
                        position += signature.header.size();
                        continue;
                    }

                    file.hasValidFooter = true;
                    file.quality = RecoveryQuality::Good;
                }
            }
            else
            {
                file.size =
                    qint64(signature.maxSizeMb) *
                    1024 *
                    1024;

                file.hasValidFooter = false;
                file.quality = RecoveryQuality::Regular;
            }

            results.push_back(file);

            position += qMax(1, signature.header.size());
        }
    }

    return results;
}