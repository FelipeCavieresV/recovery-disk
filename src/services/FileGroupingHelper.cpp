#include "FileGroupingHelper.h"

#include "FileTypeHelper.h"

#include <QMap>
#include <QTreeWidgetItem>

void FileGroupingHelper::populateTree(
    QTreeWidget* tree,
    const QVector<RecoverableFile>& files
)
{
    if (!tree)
        return;

    tree->clear();

    auto* allItem = new QTreeWidgetItem(tree);
    allItem->setText(0, QString("Todos (%1)").arg(files.size()));
    allItem->setData(0, Qt::UserRole, "all");
    allItem->setData(0, Qt::UserRole + 1, "");

    QMap<QString, int> categoryCount;
    QMap<QString, QMap<QString, int>> extensionCount;

    for (const RecoverableFile& file : files)
    {
        QString category =
            FileTypeHelper::typeKey(file.extension);

        QString extension =
            file.extension.toLower().trimmed();

        if (extension.isEmpty())
            extension = "sin_extension";

        categoryCount[category]++;
        extensionCount[category][extension]++;
    }

    for (const QString& category : FileTypeHelper::categoryKeys())
    {
        if (category == "all")
            continue;

        int total =
            categoryCount.value(category, 0);

        auto* categoryItem =
            new QTreeWidgetItem(tree);

        categoryItem->setText(
            0,
            QString("%1 (%2)")
                .arg(FileTypeHelper::categoryTitle(category))
                .arg(total)
        );

        categoryItem->setData(0, Qt::UserRole, category);
        categoryItem->setData(0, Qt::UserRole + 1, "");

        QMap<QString, int> extMap =
            extensionCount.value(category);

        for (
            auto it = extMap.begin();
            it != extMap.end();
            ++it
        )
        {
            auto* extensionItem =
                new QTreeWidgetItem(categoryItem);

            extensionItem->setText(
                0,
                QString("%1 (%2)")
                    .arg(it.key().toUpper())
                    .arg(it.value())
            );

            extensionItem->setData(0, Qt::UserRole, category);
            extensionItem->setData(0, Qt::UserRole + 1, it.key());
        }
    }

    tree->expandAll();

    if (tree->topLevelItemCount() > 0)
        tree->setCurrentItem(tree->topLevelItem(0));
}