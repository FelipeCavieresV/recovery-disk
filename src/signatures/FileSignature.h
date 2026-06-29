#pragma once

#include <QString>
#include <QByteArray>

struct FileSignature
{
    QString extension;

    QByteArray header;
    QByteArray footer;

    // Bytes adicionales que deben aparecer en un offset fijo desde el inicio
    // (para distinguir RIFF+WAVE de RIFF+AVI, ftyp variants, etc.)
    QByteArray subHeader;
    int subHeaderOffset = 0;

    bool hasFooter = true;

    int maxSizeMb = 100;
};