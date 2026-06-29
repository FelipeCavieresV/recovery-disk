#include "FileTypeDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QPainter>
#include <QStyleOption>

FileTypeDialog::FileTypeDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Tipo de archivos a recuperar");
    setModal(true);
    setFixedSize(700, 420);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    setStyleSheet(R"(
        QDialog {
            background-color: #ffffff;
        }
        QLabel#title {
            font-size: 20px;
            font-weight: bold;
            color: #1a1a2e;
        }
        QLabel#subtitle {
            font-size: 13px;
            color: #888888;
        }
    )");

    buildUi();
}

void FileTypeDialog::buildUi()
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(36, 32, 36, 28);
    root->setSpacing(0);

    // --- Encabezado ---
    QLabel* title = new QLabel("¿Qué tipo de archivos desea recuperar?", this);
    title->setObjectName("title");

    QLabel* subtitle = new QLabel("Recuperar los archivos perdidos fácilmente y rápidamente", this);
    subtitle->setObjectName("subtitle");

    root->addWidget(title);
    root->addSpacing(4);
    root->addWidget(subtitle);
    root->addSpacing(24);

    // --- Grid de tarjetas (2 filas × 3 columnas) ---
    QGridLayout* grid = new QGridLayout();
    grid->setSpacing(12);

    struct CardDef {
        QString key;
        QString icon;
        QString title;
        QString subtitle;
        QString color;
    };

    QList<CardDef> defs = {
        { "images",    "🖼",  "Foto",                  "jpg, png, cr2, nef, etc.",              "#FF8C42" },
        { "videos",    "🎬",  "Video",                 "mp4, mov, avi, flv, etc.",              "#7B5EA7" },
        { "documents", "📄",  "Documento",             "pdf, xls, doc, txt, etc.",              "#3DAA6B" },
        { "audio",     "🎵",  "Audio",                 "mp3, aac, wav, ape, etc.",              "#E84855" },
        { "others",    "⭐",  "Otro",                  "Otros tipos",                           "#F5A623" },
        { "system",    "⚙",  "Archivos del Sistema",  "Controladores, configuración, etc.",    "#5B8DB8" },
    };

    int col = 0, row = 0;
    for (int i = 0; i < defs.size(); ++i)
    {
        const auto& d = defs[i];

        QPushButton* btn = createCard(d.icon, d.title, d.subtitle, d.color);

        CategoryCard card;
        card.key = d.key;
        card.button = btn;
        card.selected = true;
        m_cards.append(card);

        const int idx = i;
        connect(btn, &QPushButton::clicked, this, [this, idx]() {
            toggleCard(idx);
        });

        grid->addWidget(btn, row, col);
        ++col;
        if (col == 3) { col = 0; ++row; }
    }

    root->addLayout(grid);
    root->addSpacing(24);

    // --- Botón principal ---
    m_scanButton = new QPushButton("Escanear tipos de archivos seleccionados", this);
    m_scanButton->setFixedHeight(44);
    m_scanButton->setStyleSheet(R"(
        QPushButton {
            background-color: #2563EB;
            color: white;
            font-size: 14px;
            font-weight: bold;
            border: none;
            border-radius: 8px;
        }
        QPushButton:hover {
            background-color: #1D4ED8;
        }
        QPushButton:pressed {
            background-color: #1E40AF;
        }
        QPushButton:disabled {
            background-color: #BFBFBF;
        }
    )");

    connect(m_scanButton, &QPushButton::clicked, this, &QDialog::accept);
    root->addWidget(m_scanButton);

    updateScanButton();
}

QPushButton* FileTypeDialog::createCard(
    const QString& icon,
    const QString& title,
    const QString& subtitle,
    const QString& accentColor)
{
    QPushButton* btn = new QPushButton(this);
    btn->setFixedHeight(86);
    btn->setCheckable(false);
    btn->setProperty("selected", true);
    btn->setProperty("accentColor", accentColor);

    // Layout interno del botón (ícono + texto)
    QHBoxLayout* h = new QHBoxLayout(btn);
    h->setContentsMargins(14, 10, 14, 10);
    h->setSpacing(12);

    QLabel* iconLabel = new QLabel(icon, btn);
    iconLabel->setFixedSize(36, 36);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setStyleSheet(
        QString("font-size: 22px; background: %1; border-radius: 8px;").arg(accentColor)
    );

    QVBoxLayout* texts = new QVBoxLayout();
    texts->setSpacing(2);

    QLabel* titleLabel = new QLabel(title, btn);
    titleLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #1a1a2e; background: transparent;");

    QLabel* subtitleLabel = new QLabel(subtitle, btn);
    subtitleLabel->setStyleSheet("font-size: 11px; color: #888888; background: transparent;");

    texts->addWidget(titleLabel);
    texts->addWidget(subtitleLabel);

    h->addWidget(iconLabel);
    h->addLayout(texts);
    h->addStretch();

    // Checkmark en la esquina superior derecha
    QLabel* checkLabel = new QLabel("✓", btn);
    checkLabel->setObjectName("checkmark");
    checkLabel->setFixedSize(18, 18);
    checkLabel->setAlignment(Qt::AlignCenter);
    checkLabel->setStyleSheet(R"(
        font-size: 11px;
        font-weight: bold;
        color: white;
        background-color: #2563EB;
        border-radius: 9px;
    )");

    // Posicionar el checkmark en la esquina superior derecha del botón
    checkLabel->setParent(btn);
    checkLabel->raise();

    // Aplicar estilo seleccionado por defecto
    btn->setStyleSheet(R"(
        QPushButton {
            background-color: #EFF6FF;
            border: 2px solid #2563EB;
            border-radius: 10px;
            text-align: left;
        }
        QPushButton:hover {
            background-color: #DBEAFE;
        }
    )");

    return btn;
}

void FileTypeDialog::toggleCard(int index)
{
    auto& card = m_cards[index];
    card.selected = !card.selected;

    QPushButton* btn = card.button;

    if (card.selected)
    {
        btn->setStyleSheet(R"(
            QPushButton {
                background-color: #EFF6FF;
                border: 2px solid #2563EB;
                border-radius: 10px;
                text-align: left;
            }
            QPushButton:hover {
                background-color: #DBEAFE;
            }
        )");
    }
    else
    {
        btn->setStyleSheet(R"(
            QPushButton {
                background-color: #F9FAFB;
                border: 2px solid #D1D5DB;
                border-radius: 10px;
                text-align: left;
            }
            QPushButton:hover {
                background-color: #F3F4F6;
            }
        )");
    }

    // Mostrar u ocultar el checkmark
    QLabel* check = btn->findChild<QLabel*>("checkmark");
    if (check)
        check->setVisible(card.selected);

    updateScanButton();
}

void FileTypeDialog::updateScanButton()
{
    bool anySelected = false;
    for (const auto& c : m_cards)
    {
        if (c.selected) { anySelected = true; break; }
    }
    m_scanButton->setEnabled(anySelected);
}

void FileTypeDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);

    // Reposicionar los checkmarks en la esquina superior derecha de cada tarjeta
    for (const auto& card : m_cards)
    {
        QLabel* check = card.button ? card.button->findChild<QLabel*>("checkmark") : nullptr;
        if (check)
        {
            QSize btnSize = card.button->size();
            check->move(btnSize.width() - check->width() - 6, 6);
        }
    }
}

QSet<QString> FileTypeDialog::selectedCategories() const
{
    QSet<QString> result;
    for (const auto& c : m_cards)
    {
        if (c.selected)
            result.insert(c.key);
    }
    return result;
}
