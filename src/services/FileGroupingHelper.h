#pragma once

#include <QTreeWidget>
#include <QVector>

#include "models/RecoverableFile.h"

class FileGroupingHelper
{
public:
    static void populateTree(
        QTreeWidget* tree,
        const QVector<RecoverableFile>& files
    );
};