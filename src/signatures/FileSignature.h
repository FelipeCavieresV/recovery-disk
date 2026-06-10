#pragma once

#include <QString>
#include <QByteArray>

struct FileSignature
{
    QString extension;

    QByteArray header;
    QByteArray footer;

    bool hasFooter = true;

    int maxSizeMb = 100;
};