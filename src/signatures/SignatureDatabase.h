#pragma once

#include <QVector>

#include "FileSignature.h"

class SignatureDatabase
{
public:

    static QVector<FileSignature> loadDefaultSignatures();
};