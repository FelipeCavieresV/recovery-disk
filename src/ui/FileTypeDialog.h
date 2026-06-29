#pragma once

#include <QDialog>
#include <QResizeEvent>
#include <QSet>
#include <QString>

class QPushButton;

class FileTypeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FileTypeDialog(QWidget* parent = nullptr);

    // Categorías seleccionadas por el usuario ("images", "videos", "documents",
    // "audio", "others", "system"). Vacío = todas (no debería pasar porque
    // el botón se activa solo cuando hay al menos una seleccionada).
    QSet<QString> selectedCategories() const;

private:
    struct CategoryCard
    {
        QString key;
        QPushButton* button = nullptr;
        bool selected = true;
    };

    QList<CategoryCard> m_cards;
    QPushButton* m_scanButton = nullptr;

    void buildUi();
    void toggleCard(int index);
    void updateScanButton();

protected:
    void resizeEvent(QResizeEvent* event) override;

    QPushButton* createCard(
        const QString& icon,
        const QString& title,
        const QString& subtitle,
        const QString& accentColor
    );
};
