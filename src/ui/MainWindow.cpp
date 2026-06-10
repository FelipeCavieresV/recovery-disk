#include "MainWindow.h"

#include "app/ScanWorker.h"
#include "services/RecycleBinScanner.h"
#include <QFileInfo>
#include <QUrl>
#include <QPixmap>
#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include "app/ScanWorker.h"
#include "services/RecycleBinScanner.h"
#include "services/FolderScanner.h"
#include "services/FileRecoveryService.h"
#include "services/FileTypeHelper.h"
#include "services/FileGroupingHelper.h"
#include "preview/PreviewService.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QFrame>
#include <QSplitter>
#include <QFileDialog>
#include <QMessageBox>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QStorageInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QCheckBox>
#include <QPixmap>
#include <QMetaType>
#include <QAbstractItemView>
#include <QMetaObject>
#include <QFileInfo>
#include <QGroupBox>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QTreeWidgetItem>
#include <QStorageInfo>
#include <QBrush>
#include <QColor>
#include <QTimer>
#include <QScrollBar>
#include <functional>
#include <QMap>
#include <QSet>
#include <QListWidget>
#include <QListWidgetItem>
#include <QListView>
#include <QIcon>
#include <QPainter>
#include <QImageReader>
#include <QPainter>
#include <QPolygon>
#include <QScrollBar>
#include <QFileInfo>


namespace
{
    QString normalizedExtension(QString extension)
    {
        extension = extension.trimmed().toLower();

        if (extension.startsWith("."))
            extension.remove(0, 1);

        return extension;
    }

    QString categoryKeyForExtension(const QString& extension)
    {
        const QString ext = normalizedExtension(extension);

        static const QSet<QString> images =
        {
            "jpg", "jpeg", "png", "gif", "bmp", "webp", "tif", "tiff", "heic", "raw"
        };

        static const QSet<QString> videos =
        {
            "mp4", "mov", "avi", "mkv", "wmv", "flv", "webm", "mpeg", "mpg", "3gp"
        };

        static const QSet<QString> documents =
        {
            "pdf", "doc", "docx", "xls", "xlsx", "ppt", "pptx", "txt", "csv",
            "rtf", "odt", "ods", "odp", "xml", "json"
        };

        static const QSet<QString> audio =
        {
            "mp3", "wav", "aac", "flac", "ogg", "m4a", "wma"
        };

        static const QSet<QString> compressed =
        {
            "zip", "rar", "7z", "tar", "gz", "bz2", "xz"
        };

        if (images.contains(ext))
            return "images";

        if (videos.contains(ext))
            return "videos";

        if (documents.contains(ext))
            return "documents";

        if (audio.contains(ext))
            return "audio";

        if (compressed.contains(ext))
            return "compressed";

        return "others";
    }

    QString extensionForFile(const RecoverableFile& file)
    {
        QString ext = normalizedExtension(file.extension);

        if (!ext.isEmpty()) {
            return ext;
        }

        ext = normalizedExtension(QFileInfo(file.originalName).suffix());

        if (!ext.isEmpty()) {
            return ext;
        }

        ext = normalizedExtension(QFileInfo(file.originalPath).suffix());

        return ext;
    }

    QString categoryKeyForFile(const RecoverableFile& file)
    {
        return categoryKeyForExtension(extensionForFile(file));
    }

    QString categoryLabelForKey(const QString& key)
    {
        if (key == "images")
            return "Imágenes";

        if (key == "videos")
            return "Videos";

        if (key == "documents")
            return "Documentos";

        if (key == "audio")
            return "Audio";

        if (key == "compressed")
            return "Comprimidos";

        if (key == "others")
            return "Otros";

        return "Todos";
    }
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    qRegisterMetaType<RecoverableFile>("RecoverableFile");
    qRegisterMetaType<QVector<RecoverableFile>>("QVector<RecoverableFile>");

    setupUi();
    loadDrives();
    updateSummary();
    clearPreviewDetails();

    scanFlushTimer = new QTimer(this);
    scanFlushTimer->setInterval(250);

    connect(
        scanFlushTimer,
        &QTimer::timeout,
        this,
        &MainWindow::flushPendingScanFiles
    );

    treeUpdateTimer = new QTimer(this);
    treeUpdateTimer->setInterval(700);

    connect(
        treeUpdateTimer,
        &QTimer::timeout,
        this,
        [this]()
        {
            updateResultsTree();
        }
    );
}

MainWindow::~MainWindow()
{
    if (scanWorker)
        scanWorker->cancel();

    if (scanThread)
    {
        scanThread->quit();
        scanThread->wait();
    }
}

void MainWindow::setupUi()
{
    setWindowTitle("RecoveryDisk - Recuperación de archivos");
    resize(1320, 780);

    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    QHBoxLayout* rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    sidebar = createSidebar();

    pages = new QStackedWidget(this);
    homePage = createHomePage();
    resultsPage = createResultsPage();

    pages->addWidget(homePage);
    pages->addWidget(resultsPage);

    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(pages, 1);

    connect(homeButton, &QPushButton::clicked, this, [this]()
    {
        pages->setCurrentWidget(homePage);
    });

    connect(resultsButton, &QPushButton::clicked, this, [this]()
    {
        pages->setCurrentWidget(resultsPage);
    });

    connect(recycleBinCard, &QPushButton::clicked, this, [this]()
    {
        pages->setCurrentWidget(resultsPage);
        scanRecycleBin();
    });

    connect(folderCard, &QPushButton::clicked, this, [this]()
    {
        pages->setCurrentWidget(resultsPage);
        scanFolder();
    });

    connect(deepScanCard, &QPushButton::clicked, this, [this]()
    {
        pages->setCurrentWidget(resultsPage);
        deepScan();
    });

    loadDrives();
    setupResultsTree();
    clearPreviewDetails();
    setScanningState(false);
}

QWidget* MainWindow::createSidebar()
{
    QFrame* panel = new QFrame(this);
    panel->setFixedWidth(230);
    panel->setObjectName("Sidebar");

    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(18, 22, 18, 18);
    layout->setSpacing(12);

    QLabel* logo = new QLabel("RecoveryDisk");
    logo->setObjectName("LogoLabel");

    QLabel* subtitle = new QLabel("Data Recovery");
    subtitle->setObjectName("LogoSubtitle");

    homeButton = createSidebarButton("Inicio");
    resultsButton = createSidebarButton("Resultados");

    layout->addWidget(logo);
    layout->addWidget(subtitle);
    layout->addSpacing(25);
    layout->addWidget(homeButton);
    layout->addWidget(resultsButton);
    layout->addStretch();

    QLabel* version = new QLabel("v1.0");
    version->setObjectName("VersionLabel");
    layout->addWidget(version);

    panel->setStyleSheet(R"(
        QFrame#Sidebar {
            background-color: #101828;
        }

        QLabel#LogoLabel {
            color: white;
            font-size: 24px;
            font-weight: 700;
        }

        QLabel#LogoSubtitle {
            color: #98A2B3;
            font-size: 13px;
        }

        QLabel#VersionLabel {
            color: #667085;
            font-size: 12px;
        }
    )");

    return panel;
}

QPushButton* MainWindow::createSidebarButton(const QString& text)
{
    QPushButton* button = new QPushButton(text);
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(44);

    button->setStyleSheet(R"(
        QPushButton {
            background-color: transparent;
            color: #D0D5DD;
            border: none;
            border-radius: 10px;
            text-align: left;
            padding-left: 14px;
            font-size: 15px;
            font-weight: 500;
        }

        QPushButton:hover {
            background-color: #1D2939;
            color: white;
        }

        QPushButton:pressed {
            background-color: #344054;
        }
    )");

    return button;
}

QWidget* MainWindow::createHomePage()
{
    QWidget* page = new QWidget(this);

    QVBoxLayout* mainLayout = new QVBoxLayout(page);
    mainLayout->setContentsMargins(36, 30, 36, 30);
    mainLayout->setSpacing(24);

    QLabel* title = new QLabel("¿Qué quieres recuperar?");
    title->setStyleSheet("font-size: 30px; font-weight: 700; color: #101828;");

    QLabel* description = new QLabel(
        "Selecciona una ubicación para iniciar el escaneo de archivos eliminados o perdidos."
    );
    description->setStyleSheet("font-size: 15px; color: #667085;");

    mainLayout->addWidget(title);
    mainLayout->addWidget(description);

    QFrame* drivePanel = new QFrame(this);
    drivePanel->setObjectName("DrivePanel");

    QHBoxLayout* driveLayout = new QHBoxLayout(drivePanel);
    driveLayout->setContentsMargins(20, 16, 20, 16);
    driveLayout->setSpacing(12);

    QLabel* driveLabel = new QLabel("Unidad para escaneo profundo:");
    driveLabel->setStyleSheet("font-size: 14px; color: #344054; font-weight: 600;");

    driveCombo = new QComboBox(this);
    driveCombo->setMinimumHeight(38);
    driveCombo->setMinimumWidth(180);

    driveLayout->addWidget(driveLabel);
    driveLayout->addWidget(driveCombo);
    driveLayout->addStretch();

    mainLayout->addWidget(drivePanel);

    QGridLayout* cardGrid = new QGridLayout();
    cardGrid->setSpacing(20);

    recycleBinCard = createScanCard(
        "Papelera de reciclaje",
        "Busca archivos eliminados desde la papelera.",
        "🗑"
    );

    folderCard = createScanCard(
        "Carpeta específica",
        "Escanea una carpeta seleccionada manualmente.",
        "📁"
    );

    deepScanCard = createScanCard(
        "Escaneo profundo",
        "Analiza la unidad seleccionada por firmas de archivo.",
        "💽"
    );

    cardGrid->addWidget(recycleBinCard, 0, 0);
    cardGrid->addWidget(folderCard, 0, 1);
    cardGrid->addWidget(deepScanCard, 1, 0, 1, 2);

    mainLayout->addLayout(cardGrid);
    mainLayout->addStretch();

    page->setStyleSheet(R"(
        QWidget {
            background-color: #F9FAFB;
        }

        QFrame#DrivePanel {
            background-color: white;
            border: 1px solid #EAECF0;
            border-radius: 14px;
        }

        QComboBox {
            background-color: white;
            color: #101828;
            border: 1px solid #D0D5DD;
            border-radius: 8px;
            padding: 6px 10px;
            font-size: 14px;
        }

        QComboBox QAbstractItemView {
            background-color: white;
            color: #101828;
            selection-background-color: #D1E9FF;
            selection-color: #101828;
        }
    )");

    return page;
}

QPushButton* MainWindow::createScanCard(
    const QString& title,
    const QString& subtitle,
    const QString& icon
)
{
    QPushButton* card = new QPushButton();
    card->setCursor(Qt::PointingHandCursor);
    card->setMinimumSize(300, 150);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    card->setText(
        icon + "\n\n" +
        title + "\n" +
        subtitle
    );

    card->setStyleSheet(R"(
        QPushButton {
            background-color: white;
            border: 1px solid #EAECF0;
            border-radius: 18px;
            color: #101828;
            text-align: left;
            padding: 24px;
            font-size: 15px;
            font-weight: 600;
        }

        QPushButton:hover {
            border: 2px solid #1570EF;
            background-color: #EFF8FF;
        }

        QPushButton:pressed {
            background-color: #D1E9FF;
        }
    )");

    return card;
}

QWidget* MainWindow::createResultsPage()
{
    QWidget* page = new QWidget(this);

    QVBoxLayout* root = new QVBoxLayout(page);
    root->setContentsMargins(24, 22, 24, 18);
    root->setSpacing(16);

    QLabel* title = new QLabel("Resultados de recuperación");
    title->setStyleSheet("font-size: 26px; font-weight: 700; color: #101828;");

    QLabel* subtitle = new QLabel(
        "Revisa los archivos encontrados, filtra por tipo y recupera los seleccionados."
    );
    subtitle->setStyleSheet("font-size: 14px; color: #667085;");

    root->addWidget(title);
    root->addWidget(subtitle);

    QFrame* toolbar = new QFrame(this);
    toolbar->setObjectName("Toolbar");

    QHBoxLayout* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(14, 12, 14, 12);
    toolbarLayout->setSpacing(10);

    scanRecycleBinButton = new QPushButton("Papelera");
    scanFolderButton = new QPushButton("Carpeta");
    deepScanButton = new QPushButton("Escaneo profundo");
    cancelScanButton = new QPushButton("Cancelar");

    viewModeCombo = new QComboBox(this);
    viewModeCombo->addItem("Lista");
    viewModeCombo->addItem("Miniaturas");
    viewModeCombo->setMinimumWidth(140);
    viewModeCombo->setMinimumHeight(36);

    searchInput = new QLineEdit();
    searchInput->setPlaceholderText("Buscar por nombre o extensión...");
    searchInput->setMinimumHeight(36);

    toolbarLayout->addWidget(scanRecycleBinButton);
    toolbarLayout->addWidget(scanFolderButton);
    toolbarLayout->addWidget(deepScanButton);
    toolbarLayout->addWidget(cancelScanButton);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(viewModeCombo);
    toolbarLayout->addWidget(searchInput, 1);

    root->addWidget(toolbar);

    QFrame* statusPanel = new QFrame(this);
    statusPanel->setObjectName("StatusPanel");

    QHBoxLayout* statusLayout = new QHBoxLayout(statusPanel);
    statusLayout->setContentsMargins(14, 10, 14, 10);
    statusLayout->setSpacing(18);

    scanStatusLabel = new QLabel("Estado: Listo");
    scanDriveLabel = new QLabel("Unidad: -");
    foundCountLabel = new QLabel("Encontrados: 0");
    selectedCountLabel = new QLabel("Seleccionados: 0");

    progressBar = new QProgressBar();
    progressBar->setMinimumWidth(240);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);

    statusLayout->addWidget(scanStatusLabel);
    statusLayout->addWidget(scanDriveLabel);
    statusLayout->addWidget(foundCountLabel);
    statusLayout->addWidget(selectedCountLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(progressBar);

    root->addWidget(statusPanel);

    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);

    QFrame* treePanel = new QFrame(this);
    treePanel->setObjectName("ContentPanel");
    treePanel->setMinimumWidth(230);

    QVBoxLayout* treeLayout = new QVBoxLayout(treePanel);
    treeLayout->setContentsMargins(12, 12, 12, 12);
    treeLayout->setSpacing(8);

    QLabel* treeTitle = new QLabel("Tipos de archivo");
    treeTitle->setStyleSheet("font-weight: 700; font-size: 14px; color: #101828;");

    resultsTree = new QTreeWidget();
    resultsTree->setHeaderHidden(true);
    resultsTree->setRootIsDecorated(true);
    resultsTree->setIndentation(18);
    resultsTree->setExpandsOnDoubleClick(true);

    setupResultsTree();

    connect(
        resultsTree,
        &QTreeWidget::itemClicked,
        this,
        &MainWindow::onTreeItemClicked
    );

    treeLayout->addWidget(treeTitle);
    treeLayout->addWidget(resultsTree, 1);

    QFrame* tablePanel = new QFrame(this);
    tablePanel->setObjectName("ContentPanel");

    QVBoxLayout* tableLayout = new QVBoxLayout(tablePanel);
    tableLayout->setContentsMargins(12, 12, 12, 12);
    tableLayout->setSpacing(10);

    QHBoxLayout* tableTopLayout = new QHBoxLayout();

    resultCountLabel = new QLabel("Resultados: 0");

    selectAllButton = new QPushButton("Seleccionar todo");
    recoverButton = new QPushButton("Recuperar");

    tableTopLayout->addWidget(resultCountLabel);
    tableTopLayout->addStretch();
    tableTopLayout->addWidget(selectAllButton);
    tableTopLayout->addWidget(recoverButton);

    resultsViews = new QStackedWidget(this);

    table = new QTableWidget();

    table->setColumnCount(8);
    table->setHorizontalHeaderLabels(
        {
            "",
            "Nombre",
            "Tipo",
            "Estado",
            "Tamaño",
            "Fecha",
            "Origen",
            "Ruta original"
        }
    );

    table->setSortingEnabled(false);
    table->setAlternatingRowColors(false);
    table->setWordWrap(false);
    table->setShowGrid(false);

    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(28);

    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);

    table->setColumnWidth(0, 36);
    table->setColumnWidth(1, 220);
    table->setColumnWidth(2, 90);
    table->setColumnWidth(3, 90);
    table->setColumnWidth(4, 90);
    table->setColumnWidth(5, 140);
    table->setColumnWidth(6, 100);
    table->setColumnWidth(7, 300);

    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    gallery = new QListWidget(this);

    gallery->setViewMode(QListView::IconMode);
    gallery->setResizeMode(QListView::Adjust);
    gallery->setMovement(QListView::Static);
    gallery->setSpacing(16);
    gallery->setWordWrap(true);
    gallery->setIconSize(QSize(96, 96));
    gallery->setGridSize(QSize(130, 145));
    gallery->setSelectionMode(QAbstractItemView::ExtendedSelection);
    gallery->setEditTriggers(QAbstractItemView::NoEditTriggers);
    gallery->setUniformItemSizes(false);
    gallery->setWrapping(true);
    gallery->setFlow(QListView::LeftToRight);
    gallery->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    gallery->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    resultsViews->addWidget(table);
    resultsViews->addWidget(gallery);

    tableLayout->addLayout(tableTopLayout);
    tableLayout->addWidget(resultsViews, 1);

    setupThumbnailLoader();

    connect(
        table->verticalScrollBar(),
        &QScrollBar::valueChanged,
        this,
        [this](int value)
        {
            QScrollBar* bar = table->verticalScrollBar();

            if (!bar)
            {
                return;
            }

            if (value >= bar->maximum() - 20)
            {
                loadMoreVisibleRows();
            }
        }
    );

    connect(
        gallery->verticalScrollBar(),
        &QScrollBar::valueChanged,
        this,
        [this]()
        {
            requestVisibleThumbnails();
        }
    );

    QFrame* previewPanel = new QFrame(this);
    previewPanel->setObjectName("ContentPanel");
    previewPanel->setMinimumWidth(270);

    QVBoxLayout* previewLayout = new QVBoxLayout(previewPanel);
    previewLayout->setContentsMargins(14, 14, 14, 14);
    previewLayout->setSpacing(10);

    QLabel* previewTitle = new QLabel("Vista previa");
    previewTitle->setStyleSheet("font-weight: 700; font-size: 15px; color: #101828;");

    previewLabel = new QLabel("Selecciona un archivo");
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setMinimumHeight(180);
    previewLabel->setWordWrap(true);
    previewLabel->setObjectName("PreviewBox");

    previewStatusLabel = new QLabel();
    previewStatusLabel->setWordWrap(true);
    previewStatusLabel->setAlignment(Qt::AlignCenter);
    previewStatusLabel->setStyleSheet("color:#667085;");

    videoWidget = new QVideoWidget(this);
    videoWidget->setMinimumHeight(180);
    videoWidget->setObjectName("PreviewBox");
    videoWidget->hide();

    mediaPlayer = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);

    mediaPlayer->setAudioOutput(audioOutput);
    mediaPlayer->setVideoOutput(videoWidget);
    audioOutput->setVolume(0.5);

    previewNameLabel = new QLabel("Nombre: -");
    previewTypeLabel = new QLabel("Tipo: -");
    previewSizeLabel = new QLabel("Tamaño: -");
    previewQualityLabel = new QLabel("Estado: -");
    previewOriginalPathLabel = new QLabel("Ruta original: -");
    previewTempPathLabel = new QLabel("Ruta temporal: -");

    previewOriginalPathLabel->setWordWrap(true);
    previewTempPathLabel->setWordWrap(true);

    openTempFileButton = new QPushButton("Abrir archivo");
    openTempFolderButton = new QPushButton("Abrir carpeta");

    previewLayout->addWidget(previewTitle);
    previewLayout->addWidget(previewLabel);
    previewLayout->addWidget(previewStatusLabel);
    previewLayout->addWidget(videoWidget);
    previewLayout->addWidget(previewNameLabel);
    previewLayout->addWidget(previewTypeLabel);
    previewLayout->addWidget(previewSizeLabel);
    previewLayout->addWidget(previewQualityLabel);
    previewLayout->addWidget(previewOriginalPathLabel);
    previewLayout->addWidget(previewTempPathLabel);
    previewLayout->addStretch();
    previewLayout->addWidget(openTempFileButton);
    previewLayout->addWidget(openTempFolderButton);

    splitter->addWidget(treePanel);
    splitter->addWidget(tablePanel);
    splitter->addWidget(previewPanel);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 4);
    splitter->setStretchFactor(2, 2);

    splitter->setSizes({ 240, 760, 320 });

    root->addWidget(splitter, 1);

    connect(scanRecycleBinButton, &QPushButton::clicked, this, &MainWindow::scanRecycleBin);
    connect(scanFolderButton, &QPushButton::clicked, this, &MainWindow::scanFolder);
    connect(deepScanButton, &QPushButton::clicked, this, &MainWindow::deepScan);
    connect(cancelScanButton, &QPushButton::clicked, this, &MainWindow::cancelDeepScan);
    connect(recoverButton, &QPushButton::clicked, this, &MainWindow::recoverSelectedFiles);
    connect(selectAllButton, &QPushButton::clicked, this, &MainWindow::toggleSelectAll);
    connect(searchInput, &QLineEdit::textChanged, this, &MainWindow::applyFilters);

    connect(
        viewModeCombo,
        &QComboBox::currentIndexChanged,
        this,
        [this](int index)
        {
            resultsViews->setCurrentIndex(index);

            if (index == 1)
            {
                QTimer::singleShot(
                    100,
                    this,
                    &MainWindow::requestVisibleThumbnails
                );
            }
        }
    );
    connect(
        table,
        &QTableWidget::cellClicked,
        this,
        [this](int row, int)
        {
            showPreview(row);
        }
    );

    connect(
        table,
        &QTableWidget::itemChanged,
        this,
        [this]()
        {
            updateSelectedCount();
        }
    );

    connect(
        gallery,
        &QListWidget::itemClicked,
        this,
        [this](QListWidgetItem* item)
        {
            if (!item)
            {
                return;
            }

            const int fileIndex = item->data(Qt::UserRole).toInt();

            showPreviewByFileIndex(fileIndex);
        }
    );

    connect(
        gallery,
        &QListWidget::itemChanged,
        this,
        [this]()
        {
            updateSelectedCount();
        }
    );

    connect(openTempFileButton, &QPushButton::clicked, this, &MainWindow::openCurrentTempFile);
    connect(openTempFolderButton, &QPushButton::clicked, this, &MainWindow::openCurrentTempFolder);

    page->setStyleSheet(R"(
        QWidget {
            background-color: #F9FAFB;
            color: #101828;
        }

        QFrame#Toolbar,
        QFrame#StatusPanel,
        QFrame#ContentPanel {
            background-color: white;
            border: 1px solid #EAECF0;
            border-radius: 14px;
        }

        QLabel {
            color: #344054;
        }

        QLineEdit {
            background-color: white;
            color: #101828;
            border: 1px solid #D0D5DD;
            border-radius: 8px;
            padding: 7px 10px;
            font-size: 14px;
        }

        QComboBox {
            background-color: white;
            color: #101828;
            border: 1px solid #D0D5DD;
            border-radius: 8px;
            padding: 7px 10px;
            font-size: 14px;
        }

        QPushButton {
            background-color: #1570EF;
            color: white;
            border: none;
            border-radius: 8px;
            padding: 8px 14px;
            font-size: 14px;
            font-weight: 600;
        }

        QPushButton:hover {
            background-color: #175CD3;
        }

        QTableWidget {
            background-color: white;
            color: #101828;
            border: none;
            gridline-color: #EAECF0;
            font-size: 13px;
            selection-background-color: #D1E9FF;
            selection-color: #101828;
        }

        QTableWidget::item {
            color: #101828;
            padding: 4px;
        }

        QHeaderView::section {
            background-color: #F2F4F7;
            color: #344054;
            border: none;
            padding: 8px;
            font-weight: 700;
        }

        QListWidget {
            background-color: white;
            color: #101828;
            border: none;
            font-size: 13px;
            selection-background-color: #D1E9FF;
            selection-color: #101828;
        }

        QListWidget::item {
            border-radius: 10px;
            padding: 6px;
        }

        QListWidget::item:hover {
            background-color: #F2F4F7;
        }

        QListWidget::item:selected {
            background-color: #D1E9FF;
            color: #101828;
        }

        QTreeWidget {
            background-color: white;
            color: #101828;
            border: none;
            font-size: 14px;
            selection-background-color: #D1E9FF;
            selection-color: #101828;
        }

        QTreeWidget::item {
            color: #101828;
            padding: 4px;
        }

        QProgressBar {
            border: 1px solid #D0D5DD;
            border-radius: 8px;
            text-align: center;
            background-color: #F9FAFB;
            color: #101828;
            height: 16px;
        }

        QProgressBar::chunk {
            border-radius: 8px;
            background-color: #1570EF;
        }

        QLabel#PreviewBox {
            background-color: #F2F4F7;
            border: 1px dashed #D0D5DD;
            border-radius: 12px;
            color: #667085;
        }

        QVideoWidget#PreviewBox {
            background-color: #000000;
            border: 1px dashed #D0D5DD;
            border-radius: 12px;
        }
    )");

    return page;
}

void MainWindow::setupResultsTree()
{
    if (!resultsTree)
        return;

    resultsTree->clear();

    auto* allItem = new QTreeWidgetItem();
    allItem->setText(0, "Todos (0)");
    allItem->setData(0, Qt::UserRole, "all");
    allItem->setData(0, Qt::UserRole + 1, "");

    resultsTree->addTopLevelItem(allItem);
    resultsTree->setCurrentItem(allItem);

    currentTreeCategoryKey = "all";
    currentTreeExtension.clear();
}

void MainWindow::updateResultsTree()
{
    if (!resultsTree) {
        return;
    }

    const QString previousCategory = currentTreeCategoryKey;
    const QString previousExtension = currentTreeExtension;

    struct CategoryBucket
    {
        QString key;
        QString label;
        int count = 0;
        QMap<QString, int> extensionCounts;
    };

    QVector<CategoryBucket> buckets =
    {
        {"images", "Imágenes", 0, {}},
        {"videos", "Videos", 0, {}},
        {"documents", "Documentos", 0, {}},
        {"audio", "Audio", 0, {}},
        {"compressed", "Comprimidos", 0, {}},
        {"others", "Otros", 0, {}}
    };

    QMap<QString, int> bucketIndex;

    for (int i = 0; i < buckets.size(); ++i) {
        bucketIndex.insert(buckets[i].key, i);
    }

    for (const RecoverableFile& file : files) {
        const QString key = categoryKeyForFile(file);
        const QString ext = extensionForFile(file);

        const int index = bucketIndex.value(
            key,
            bucketIndex.value("others")
        );

        buckets[index].count++;

        if (!ext.isEmpty()) {
            buckets[index].extensionCounts[ext]++;
        }
    }

    resultsTree->blockSignals(true);
    resultsTree->clear();

    QTreeWidgetItem* itemToSelect = nullptr;

    auto selectIfMatches = [&](QTreeWidgetItem* item) {
        if (!item || itemToSelect) {
            return;
        }

        const QString category =
            item->data(0, Qt::UserRole).toString();

        const QString extension =
            item->data(0, Qt::UserRole + 1).toString();

        if (
            category == previousCategory &&
            extension == previousExtension
        ) {
            itemToSelect = item;
        }
    };

    auto* allItem = new QTreeWidgetItem();
    allItem->setText(0, QString("Todos (%1)").arg(files.size()));
    allItem->setData(0, Qt::UserRole, "all");
    allItem->setData(0, Qt::UserRole + 1, "");
    resultsTree->addTopLevelItem(allItem);

    selectIfMatches(allItem);

    for (const CategoryBucket& bucket : buckets) {
        auto* categoryItem = new QTreeWidgetItem();
        categoryItem->setText(
            0,
            QString("%1 (%2)")
                .arg(bucket.label)
                .arg(bucket.count)
        );
        categoryItem->setData(0, Qt::UserRole, bucket.key);
        categoryItem->setData(0, Qt::UserRole + 1, "");

        resultsTree->addTopLevelItem(categoryItem);
        selectIfMatches(categoryItem);

        for (
            auto it = bucket.extensionCounts.constBegin();
            it != bucket.extensionCounts.constEnd();
            ++it
        ) {
            auto* extensionItem = new QTreeWidgetItem();
            extensionItem->setText(
                0,
                QString(".%1 (%2)")
                    .arg(it.key())
                    .arg(it.value())
            );
            extensionItem->setData(0, Qt::UserRole, bucket.key);
            extensionItem->setData(0, Qt::UserRole + 1, it.key());

            categoryItem->addChild(extensionItem);
            selectIfMatches(extensionItem);
        }
    }

    if (!itemToSelect) {
        itemToSelect = allItem;
        currentTreeCategoryKey = "all";
        currentTreeExtension.clear();
    }

    resultsTree->setCurrentItem(itemToSelect);
    resultsTree->expandAll();
    resultsTree->blockSignals(false);
}

QString MainWindow::categoryKeyForFile(const RecoverableFile& file) const
{
    QString ext = file.extension.toLower().trimmed();

    if (ext.startsWith(".")) {
        ext.remove(0, 1);
    }

    if (
        ext == "jpg" ||
        ext == "jpeg" ||
        ext == "png" ||
        ext == "gif" ||
        ext == "bmp" ||
        ext == "webp" ||
        ext == "tif" ||
        ext == "tiff"
    ) {
        return "images";
    }

    if (
        ext == "mp4" ||
        ext == "avi" ||
        ext == "mov" ||
        ext == "mkv" ||
        ext == "wmv" ||
        ext == "flv" ||
        ext == "webm" ||
        ext == "mpeg" ||
        ext == "mpg"
    ) {
        return "videos";
    }

    if (
        ext == "pdf" ||
        ext == "doc" ||
        ext == "docx" ||
        ext == "xls" ||
        ext == "xlsx" ||
        ext == "ppt" ||
        ext == "pptx" ||
        ext == "txt" ||
        ext == "csv" ||
        ext == "rtf"
    ) {
        return "documents";
    }

    if (
        ext == "mp3" ||
        ext == "wav" ||
        ext == "aac" ||
        ext == "flac" ||
        ext == "ogg" ||
        ext == "m4a" ||
        ext == "wma"
    ) {
        return "audio";
    }

    if (
        ext == "zip" ||
        ext == "rar" ||
        ext == "7z" ||
        ext == "tar" ||
        ext == "gz" ||
        ext == "bz2"
    ) {
        return "compressed";
    }

    return "others";
}

QIcon MainWindow::iconForRecoverableFile(
    const RecoverableFile& file
) const
{
    static QHash<QString, QIcon> iconCache;

    QString ext = file.extension.toUpper();

    if (ext.startsWith("."))
    {
        ext.remove(0, 1);
    }

    if (ext.isEmpty())
    {
        ext = "FILE";
    }

    if (ext.length() > 4)
    {
        ext = ext.left(4);
    }

    if (iconCache.contains(ext))
    {
        return iconCache.value(ext);
    }

    QPixmap iconPixmap(96, 96);
    iconPixmap.fill(Qt::transparent);

    QPainter painter(&iconPixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setBrush(QColor("#F2F4F7"));
    painter.setPen(QColor("#D0D5DD"));
    painter.drawRoundedRect(8, 8, 80, 80, 12, 12);

    painter.setPen(QColor("#101828"));

    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(13);
    painter.setFont(font);

    painter.drawText(
        QRect(8, 8, 80, 80),
        Qt::AlignCenter,
        ext
    );

    painter.end();

    QIcon icon(iconPixmap);
    iconCache.insert(ext, icon);

    return icon;
}

void MainWindow::appendFileToGallery(
    const RecoverableFile& file,
    int fileIndex
)
{
    if (!gallery)
    {
        return;
    }

    QListWidgetItem* item = new QListWidgetItem();

    item->setText(file.originalName);
    item->setIcon(placeholderIconForFile(file));
    item->setData(Qt::UserRole, fileIndex);
    item->setToolTip(file.originalPath);

    item->setFlags(
        Qt::ItemIsEnabled |
        Qt::ItemIsSelectable |
        Qt::ItemIsUserCheckable
    );

    item->setCheckState(Qt::Unchecked);

    gallery->addItem(item);
}

void MainWindow::setupThumbnailLoader()
{
    thumbnailTimer = new QTimer(this);
    thumbnailTimer->setInterval(30);

    connect(
        thumbnailTimer,
        &QTimer::timeout,
        this,
        &MainWindow::loadNextThumbnailBatch
    );
}

void MainWindow::requestVisibleThumbnails()
{
    if (!gallery || !thumbnailTimer)
    {
        return;
    }

    if (viewModeCombo && viewModeCombo->currentIndex() != 1)
    {
        return;
    }

    const QRect viewportRect = gallery->viewport()->rect();

    for (int i = 0; i < gallery->count(); ++i)
    {
        QListWidgetItem* item = gallery->item(i);

        if (!item)
        {
            continue;
        }

        QRect itemRect = gallery->visualItemRect(item);

        if (!itemRect.intersects(viewportRect))
        {
            continue;
        }

        const int fileIndex = item->data(Qt::UserRole).toInt();

        if (fileIndex < 0 || fileIndex >= files.size())
        {
            continue;
        }

        thumbnailPendingIndexes.insert(fileIndex);
    }

    if (!thumbnailTimer->isActive())
    {
        thumbnailTimer->start();
    }
}

void MainWindow::loadNextThumbnailBatch()
{
    if (!gallery)
    {
        return;
    }

    if (thumbnailPendingIndexes.isEmpty())
    {
        thumbnailTimer->stop();
        return;
    }

    const int maxPerBatch = 8;
    int processed = 0;

    QList<int> indexes = thumbnailPendingIndexes.values();

    for (int fileIndex : indexes)
    {
        if (processed >= maxPerBatch)
        {
            break;
        }

        thumbnailPendingIndexes.remove(fileIndex);

        if (fileIndex < 0 || fileIndex >= files.size())
        {
            continue;
        }

        QIcon icon = realThumbnailForFile(files[fileIndex]);

        for (int i = 0; i < gallery->count(); ++i)
        {
            QListWidgetItem* item = gallery->item(i);

            if (!item)
            {
                continue;
            }

            if (item->data(Qt::UserRole).toInt() == fileIndex)
            {
                item->setIcon(icon);
                break;
            }
        }

        ++processed;
    }
}

QIcon MainWindow::placeholderIconForFile(
    const RecoverableFile& file
) const
{
    QString ext = file.extension.toUpper();

    if (ext.startsWith("."))
    {
        ext.remove(0, 1);
    }

    if (ext.isEmpty())
    {
        ext = "FILE";
    }

    if (ext.length() > 4)
    {
        ext = ext.left(4);
    }

    QPixmap iconPixmap(96, 96);
    iconPixmap.fill(Qt::transparent);

    QPainter painter(&iconPixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setBrush(QColor("#F2F4F7"));
    painter.setPen(QColor("#D0D5DD"));
    painter.drawRoundedRect(8, 8, 80, 80, 12, 12);

    painter.setPen(QColor("#101828"));

    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(13);
    painter.setFont(font);

    painter.drawText(
        QRect(8, 8, 80, 80),
        Qt::AlignCenter,
        ext
    );

    painter.end();

    return QIcon(iconPixmap);
}

QIcon MainWindow::realThumbnailForFile(
    RecoverableFile& file
)
{
    PreviewService preview;

    QString path = file.currentPath;

    if (path.isEmpty() || !QFileInfo::exists(path))
    {
        if (file.source == RecoverySource::DeepScan)
        {
            path = preview.createTemporaryPreviewFile(
                file,
                activeScanRoot
            );

            if (!path.isEmpty())
            {
                file.currentPath = path;
            }
        }
        else
        {
            path = file.originalPath;
        }
    }

    if (path.isEmpty() || !QFileInfo::exists(path))
    {
        return placeholderIconForFile(file);
    }

    if (thumbnailCache.contains(path))
    {
        return thumbnailCache.value(path);
    }

    if (preview.isImage(file.extension))
    {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        reader.setScaledSize(QSize(96, 96));

        QImage image = reader.read();

        if (!image.isNull())
        {
            QPixmap pixmap = QPixmap::fromImage(image);

            QIcon icon(pixmap);
            thumbnailCache.insert(path, icon);

            return icon;
        }
    }

    if (preview.isVideo(file.extension))
    {
        QPixmap videoPixmap(96, 96);
        videoPixmap.fill(Qt::transparent);

        QPainter painter(&videoPixmap);
        painter.setRenderHint(QPainter::Antialiasing);

        painter.setBrush(QColor("#111827"));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(8, 8, 80, 80, 12, 12);

        painter.setBrush(Qt::white);

        QPolygon playIcon;
        playIcon << QPoint(40, 30)
                 << QPoint(40, 66)
                 << QPoint(68, 48);

        painter.drawPolygon(playIcon);
        painter.end();

        QIcon icon(videoPixmap);
        thumbnailCache.insert(path, icon);

        return icon;
    }

    return placeholderIconForFile(file);
}

void MainWindow::showPreviewByFileIndex(int fileIndex)
{
    if (fileIndex < 0 || fileIndex >= files.size())
        return;

    for (int row = 0; row < table->rowCount(); ++row)
    {
        QTableWidgetItem* item = table->item(row, 1);

        if (!item)
            continue;

        if (item->data(Qt::UserRole).toInt() == fileIndex)
        {
            table->selectRow(row);
            showPreview(row);
            return;
        }
    }

    currentPreviewFileIndex = fileIndex;

    RecoverableFile& file = files[fileIndex];

    PreviewService preview;

    if (mediaPlayer)
        mediaPlayer->stop();

    if (videoWidget)
        videoWidget->hide();

    previewLabel->show();
    previewLabel->setAlignment(Qt::AlignCenter);

    QString previewPath = file.currentPath;

    if (previewPath.isEmpty() || !QFileInfo::exists(previewPath))
    {
        if (file.source == RecoverySource::DeepScan)
        {
            previewPath =
                preview.createTemporaryPreviewFile(
                    file,
                    activeScanRoot
                );

            if (!previewPath.isEmpty())
                file.currentPath = previewPath;
        }
        else
        {
            previewPath = file.originalPath;
        }
    }

    if (previewPath.isEmpty() || !QFileInfo::exists(previewPath))
    {
        restoreLastPreviewImage();
        updatePreviewDetails(file);
        return;
    }

    if (preview.isImage(file.extension))
    {
        QPixmap pixmap =
            preview.createImagePreview(
                previewPath,
                previewLabel->size()
            );

        if (!pixmap.isNull())
        {
            lastPreviewPixmap = pixmap;
            lastPreviewPath = previewPath;
            hasPersistentPreview = true;

            previewLabel->clear();
            previewLabel->setPixmap(
                lastPreviewPixmap.scaled(
                    previewLabel->size(),
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation
                )
            );
        }

        updatePreviewDetails(file);
        return;
    }

    if (preview.isVideo(file.extension))
    {
        hasPersistentPreview = false;

        previewLabel->hide();

        if (videoWidget)
            videoWidget->show();

        if (mediaPlayer)
        {
            mediaPlayer->setSource(
                QUrl::fromLocalFile(previewPath)
            );

            mediaPlayer->play();
        }

        updatePreviewDetails(file);
        return;
    }

    restoreLastPreviewImage();
    updatePreviewDetails(file);
}

void MainWindow::updateResultsTreeCounts()
{
    updateResultsTree();
}

void MainWindow::loadDrives()
{
    if (!driveCombo)
    {
        return;
    }

    driveCombo->clear();

    const QList<QStorageInfo> drives = QStorageInfo::mountedVolumes();

    for (const QStorageInfo& drive : drives)
    {
        if (!drive.isValid() || !drive.isReady())
        {
            continue;
        }

        QString rootPath = drive.rootPath();

        if (rootPath.isEmpty())
        {
            continue;
        }

        QString label = rootPath;

        if (!drive.displayName().isEmpty())
        {
            label += " - " + drive.displayName();
        }

        driveCombo->addItem(label, rootPath);
    }

    if (driveCombo->count() == 0)
    {
        driveCombo->addItem("C:/", "C:/");
    }
}

void MainWindow::scanRecycleBin()
{
    QString selectedDrive = driveCombo->currentData().toString();

    if (selectedDrive.isEmpty())
    {
        selectedDrive = driveCombo->currentText();
    }

    if (selectedDrive.isEmpty())
    {
        QMessageBox::warning(
            this,
            "RecoveryDisk",
            "Selecciona una unidad primero."
        );
        return;
    }

    activeScanRoot = selectedDrive;

    clearPreviewDetails();

    files.clear();
    visibleFileIndexes.clear();

    table->blockSignals(true);
    table->setRowCount(0);
    table->blockSignals(false);

    scanStatusLabel->setText("Estado: Escaneando papelera");
    scanDriveLabel->setText("Unidad: " + selectedDrive);

    foundCountLabel->setText("Encontrados: 0");
    selectedCountLabel->setText("Seleccionados: 0");
    resultCountLabel->setText("Resultados: 0");

    previewLabel->setText(
        QString(
            "Escaneando papelera de:\n%1"
        ).arg(selectedDrive)
    );

    progressBar->setRange(0, 0);

    QApplication::setOverrideCursor(Qt::WaitCursor);

    RecycleBinScanner scanner;

    files = scanner.scanDriveRecycleBin(
        selectedDrive
    );

    QApplication::restoreOverrideCursor();

    fillTable();

    updateResultsTree();
    updateSummary();
    updateSelectedCount();

    progressBar->setRange(0, 100);
    progressBar->setValue(100);

    scanStatusLabel->setText("Estado: Finalizado");

    foundCountLabel->setText(
        QString("Encontrados: %1")
            .arg(files.size())
    );

    resultCountLabel->setText(
        QString("Resultados: %1")
            .arg(table->rowCount())
    );

    previewLabel->setText(
        QString(
            "Escaneo de papelera finalizado.\n\n"
            "Archivos encontrados: %1"
        ).arg(files.size())
    );

    if (!files.isEmpty())
    {
        showPreview(0);
    }
}

void MainWindow::scanFolder()
{
    if (scanThread)
        return;

    QString folder =
        QFileDialog::getExistingDirectory(
            this,
            "Seleccionar carpeta"
        );

    if (folder.isEmpty())
        return;

    QFileInfo info(folder);

    if (info.isRoot())
    {
        QMessageBox::information(
            this,
            "RecoveryDisk",
            "Para escanear una unidad completa utiliza Escaneo profundo."
        );

        return;
    }

    activeScanRoot = folder;
    clearPreviewDetails();

    files.clear();
    visibleFileIndexes.clear();
    table->setRowCount(0);
    updateResultsTree();
    updateSummary();

    progressBar->setRange(0, 0);

    scanStatusLabel->setText("Estado: Escaneando carpeta");
    scanDriveLabel->setText("Carpeta: " + folder);

    previewLabel->setText(
        "Escaneando carpeta...\n"
        "La interfaz seguirá funcionando."
    );

    resultCountLabel->setText("Resultados: 0");

    setScanningState(true);
    cancelScanButton->setEnabled(false);

    scanThread = QThread::create(
        [this, folder]()
        {
            FolderScanner scanner;

            QVector<RecoverableFile> result =
                scanner.scanFolder(folder);

            QMetaObject::invokeMethod(
                this,
                [this, result]()
                {
                    files = result;

                    progressBar->setRange(0, 100);
                    progressBar->setValue(100);

                    fillTable();
                    updateResultsTree();
                    updateSummary();

                    scanStatusLabel->setText("Estado: Finalizado");

                    if (previewStatusLabel)
                    {
                        previewStatusLabel->setText(
                            QString("Escaneo finalizado. Archivos encontrados: %1")
                                .arg(files.size())
                        );
                    }

                    setScanningState(false);
                    scanThread = nullptr;
                },
                Qt::QueuedConnection
            );
        }
    );

    connect(
        scanThread,
        &QThread::finished,
        scanThread,
        &QObject::deleteLater
    );

    scanThread->start();
}

void MainWindow::deepScan()
{
    if (scanThread)
        return;

    QString selectedDrive = driveCombo->currentData().toString();

    if (selectedDrive.isEmpty())
        selectedDrive = driveCombo->currentText();

    if (selectedDrive.isEmpty())
    {
        QMessageBox::warning(
            this,
            "RecoveryDisk",
            "Selecciona una unidad primero."
        );
        return;
    }

    QString visibleDrive = selectedDrive;
    activeScanRoot = visibleDrive;

    clearPreviewDetails();

    QString workerDrive = selectedDrive.trimmed();
    workerDrive.replace("/", "\\");

    if (workerDrive.endsWith("\\"))
        workerDrive.chop(1);

    files.clear();
    pendingScanFiles.clear();
    visibleFileIndexes.clear();

    loadedVisibleRows = 0;
    currentPreviewFileIndex = -1;

    scanCompletionPending = false;
    scanWasCancelled = false;

    table->blockSignals(true);
    table->setUpdatesEnabled(false);
    table->setSortingEnabled(false);
    table->setRowCount(0);
    table->setUpdatesEnabled(true);
    table->blockSignals(false);

    updateResultsTree();
    updateSummary();
    updateSelectedCount();

    progressBar->setRange(0, 100);
    progressBar->setValue(0);

    scanStatusLabel->setText("Estado: Escaneando 0%");
    scanDriveLabel->setText("Unidad: " + visibleDrive);
    foundCountLabel->setText("Encontrados: 0");
    selectedCountLabel->setText("Seleccionados: 0");
    resultCountLabel->setText("Resultados visibles: 0 / cargados: 0");

    if (previewStatusLabel)
    {
        previewStatusLabel->setText(
            "Ejecutando escaneo profundo en: " + visibleDrive
        );
    }

    setScanningState(true);

    if (!scanFlushTimer)
    {
        scanFlushTimer = new QTimer(this);

        connect(
            scanFlushTimer,
            &QTimer::timeout,
            this,
            &MainWindow::flushPendingScanFiles
        );
    }

    scanFlushTimer->setInterval(80);
    scanFlushTimer->start();

    scanThread = new QThread(this);
    scanWorker = new ScanWorker(workerDrive);

    scanWorker->moveToThread(scanThread);

    connect(
        scanThread,
        &QThread::started,
        scanWorker,
        &ScanWorker::start
    );

    connect(
        scanWorker,
        &ScanWorker::progressChanged,
        this,
        [this](int progress)
        {
            progressBar->setValue(progress);

            scanStatusLabel->setText(
                QString("Estado: Escaneando %1%").arg(progress)
            );
        },
        Qt::QueuedConnection
    );

    connect(
        scanWorker,
        &ScanWorker::filesFoundBatch,
        this,
        [this](const QVector<RecoverableFile>& batch)
        {
            if (batch.isEmpty())
                return;

            pendingScanFiles += batch;

            const int totalFound =
                files.size() + pendingScanFiles.size();

            foundCountLabel->setText(
                QString("Encontrados: %1")
                    .arg(totalFound)
            );

            if (scanFlushTimer && !scanFlushTimer->isActive())
                scanFlushTimer->start();



            if (scanFlushTimer && !scanFlushTimer->isActive())
                scanFlushTimer->start();
        },
        Qt::QueuedConnection
    );

    connect(
        scanWorker,
        &ScanWorker::finished,
        this,
        [this]()
        {
            scanCompletionPending = true;
            scanWasCancelled = false;

            if (scanFlushTimer && !scanFlushTimer->isActive())
                scanFlushTimer->start();
        },
        Qt::QueuedConnection
    );

    connect(
        scanWorker,
        &ScanWorker::cancelled,
        this,
        [this]()
        {
            scanCompletionPending = true;
            scanWasCancelled = true;

            if (scanFlushTimer && !scanFlushTimer->isActive())
                scanFlushTimer->start();
        },
        Qt::QueuedConnection
    );

    connect(
        scanWorker,
        &ScanWorker::failed,
        this,
        [this](const QString& error)
        {
            scanCompletionPending = false;
            scanWasCancelled = false;

            pendingScanFiles.clear();

            if (scanFlushTimer)
                scanFlushTimer->stop();

            QMessageBox::critical(
                this,
                "RecoveryDisk",
                error
            );

            scanStatusLabel->setText("Estado: Error");
            previewLabel->setText(error);

            setScanningState(false);

            if (scanThread)
                scanThread->quit();
        },
        Qt::QueuedConnection
    );

    connect(
        scanThread,
        &QThread::finished,
        scanWorker,
        &QObject::deleteLater
    );

    connect(
        scanThread,
        &QThread::finished,
        scanThread,
        &QObject::deleteLater
    );

    connect(
        scanThread,
        &QThread::finished,
        this,
        [this]()
        {
            scanThread = nullptr;
            scanWorker = nullptr;
        }
    );

    scanThread->start();
}

void MainWindow::cancelDeepScan()
{
    if (scanWorker)
    {
        scanStatusLabel->setText("Estado: Cancelando...");
        scanWorker->cancel();
    }
}

void MainWindow::recoverSelectedFiles()
{
    QVector<RecoverableFile> selected = selectedFiles();

    if (selected.isEmpty())
    {
        QMessageBox::warning(
            this,
            "Sin selección",
            "Selecciona al menos un archivo."
        );
        return;
    }

    QString destination =
        QFileDialog::getExistingDirectory(
            this,
            "Seleccionar carpeta destino"
        );

    if (destination.isEmpty())
        return;

    if (isUnsafeRecoveryDestination(destination))
    {
        QMessageBox::warning(
            this,
            "Destino no recomendado",
            "No se recomienda recuperar archivos en la misma unidad o carpeta escaneada.\n\n"
            "Esto puede sobrescribir datos que aún podrían recuperarse.\n\n"
            "Selecciona otro disco, USB o carpeta externa."
        );

        return;
    }

    FileRecoveryService service;

    RecoveryResult result =
        service.recoverFiles(
            selected,
            destination
        );

    QString message =
        QString(
            "Recuperación finalizada.\n\n"
            "Total seleccionados: %1\n"
            "Recuperados: %2\n"
            "Fallidos: %3\n\n"
            "Carpeta destino:\n%4\n\n"
            "Reporte:\n%5"
        )
        .arg(result.total)
        .arg(result.recovered)
        .arg(result.failed)
        .arg(result.outputFolder)
        .arg(result.reportPath);

    if (result.failed == 0)
    {
        QMessageBox::information(
            this,
            "RecoveryDisk",
            message
        );
    }
    else
    {
        QMessageBox::warning(
            this,
            "RecoveryDisk",
            message
        );
    }
}

void MainWindow::fillTable()
{
    if (!table) {
        return;
    }

    table->blockSignals(true);
    table->setUpdatesEnabled(false);
    table->setSortingEnabled(false);
    table->clearContents();
    table->setRowCount(0);

    visibleFileIndexes.clear();
    loadedVisibleRows = 0;

    for (int i = 0; i < files.size(); ++i) {
        if (matchesCurrentFilters(files[i])) {
            visibleFileIndexes.append(i);
        }
    }

    table->setUpdatesEnabled(true);
    table->blockSignals(false);

    loadMoreVisibleRows();

    if (resultCountLabel) {
        resultCountLabel->setText(
            QString("Resultados visibles: %1 / cargados: %2")
                .arg(visibleFileIndexes.size())
                .arg(table->rowCount())
        );
    }

    updateSelectedCount();
}



void MainWindow::appendFileToTable(
    const RecoverableFile& file,
    int fileIndex
)
{
    const int row = table->rowCount();

    table->insertRow(row);

    auto* checkItem = new QTableWidgetItem();
    checkItem->setFlags(
        Qt::ItemIsUserCheckable |
        Qt::ItemIsEnabled |
        Qt::ItemIsSelectable
    );
    checkItem->setCheckState(Qt::Unchecked);
    checkItem->setData(Qt::UserRole, fileIndex);

    table->setItem(row, 0, checkItem);

    auto* nameItem = new QTableWidgetItem(file.originalName);
    nameItem->setData(Qt::UserRole, fileIndex);

    table->setItem(row, 1, nameItem);
    table->setItem(row, 2, new QTableWidgetItem(FileTypeHelper::typeName(file.extension)));
    table->setItem(row, 3, new QTableWidgetItem(FileTypeHelper::recoveryQualityText(file)));
    table->setItem(row, 4, new QTableWidgetItem(formatSize(file.size)));
    table->setItem(row, 5, new QTableWidgetItem(file.deletedAt.toString("dd/MM/yyyy HH:mm")));
    table->setItem(row, 6, new QTableWidgetItem(sourceToText(file.source)));
    table->setItem(row, 7, new QTableWidgetItem(file.originalPath));
}

void MainWindow::appendFilesToTable(
    const QVector<RecoverableFile>& newFiles
)
{
    int startIndex = files.size();

    files.append(newFiles);

    for (int i = 0; i < newFiles.size(); ++i)
    {
        if (matchesCurrentFilters(newFiles[i]))
            appendFileToTable(newFiles[i], startIndex + i);
    }

    updateResultsTree();
    updateSummary();

    resultCountLabel->setText(
        QString("Resultados visibles: %1").arg(table->rowCount())
    );
}

void MainWindow::applyFilters()
{
    if (!table)
        return;

    table->blockSignals(true);
    table->setUpdatesEnabled(false);
    table->setSortingEnabled(false);
    table->setRowCount(0);

    if (gallery)
    {
        gallery->blockSignals(true);
        gallery->setUpdatesEnabled(false);
        gallery->clear();
    }

    visibleFileIndexes.clear();
    loadedVisibleRows = 0;

    for (int i = 0; i < files.size(); ++i)
    {
        const RecoverableFile& file = files[i];

        if (matchesCurrentFilters(file))
        {
            visibleFileIndexes.append(i);
        }
    }

    table->setUpdatesEnabled(true);
    table->blockSignals(false);

    if (gallery)
    {
        gallery->setUpdatesEnabled(true);
        gallery->blockSignals(false);
    }

    loadMoreVisibleRows();

    resultCountLabel->setText(
        QString("Resultados visibles: %1 / cargados: %2")
            .arg(visibleFileIndexes.size())
            .arg(table->rowCount())
    );

    updateSelectedCount();
}

void MainWindow::updateSummary()
{
    if (foundCountLabel)
    {
        foundCountLabel->setText(
            QString("Encontrados: %1").arg(files.size())
        );
    }

    updateSelectedCount();
}

void MainWindow::updateSelectedCount()
{
    int selected = selectedFiles().size();

    if (selectedCountLabel)
    {
        selectedCountLabel->setText(
            QString("Seleccionados: %1").arg(selected)
        );
    }

    if (selectAllButton)
    {
        if (table->rowCount() > 0 && selected == table->rowCount())
            selectAllButton->setText("Deseleccionar todo");
        else
            selectAllButton->setText("Seleccionar todo");
    }
}

void MainWindow::toggleSelectAll()
{
    int selected = selectedFiles().size();
    bool shouldSelect = selected != table->rowCount();

    table->blockSignals(true);

    for (int row = 0; row < table->rowCount(); ++row)
    {
        QTableWidgetItem* checkItem = table->item(row, 0);

        if (checkItem)
        {
            checkItem->setCheckState(
                shouldSelect ? Qt::Checked : Qt::Unchecked
            );
        }
    }

    table->blockSignals(false);

    updateSelectedCount();
}

bool MainWindow::matchesCurrentFilters(
    const RecoverableFile& file
) const
{
    const QString search =
        searchInput
            ? searchInput->text().toLower().trimmed()
            : "";

    const bool matchesSearch =
        search.isEmpty() ||
        file.originalName.toLower().contains(search) ||
        file.originalPath.toLower().contains(search) ||
        extensionForFile(file).contains(search);

    if (!matchesSearch) {
        return false;
    }

    if (currentTreeCategoryKey == "all") {
        return true;
    }

    const QString fileCategory = categoryKeyForFile(file);

    if (fileCategory != currentTreeCategoryKey) {
        return false;
    }

    if (!currentTreeExtension.isEmpty()) {
        return extensionForFile(file) == currentTreeExtension;
    }

    return true;
}

void MainWindow::showPreview(int row)
{
    if (row < 0 || row >= table->rowCount())
        return;

    QTableWidgetItem* item = table->item(row, 1);

    if (!item)
        return;

    int fileIndex = item->data(Qt::UserRole).toInt();

    if (fileIndex < 0 || fileIndex >= files.size())
        return;

    currentPreviewFileIndex = fileIndex;

    RecoverableFile& file = files[fileIndex];

    PreviewService preview;

    if (mediaPlayer)
        mediaPlayer->stop();

    if (videoWidget)
        videoWidget->hide();

    previewLabel->show();
    previewLabel->setAlignment(Qt::AlignCenter);

    QString previewPath = file.currentPath;

    if (previewPath.isEmpty() || !QFileInfo::exists(previewPath))
    {
        if (file.source == RecoverySource::DeepScan)
        {
            previewPath =
                preview.createTemporaryPreviewFile(
                    file,
                    activeScanRoot
                );

            if (!previewPath.isEmpty())
                file.currentPath = previewPath;
        }
        else
        {
            previewPath = file.originalPath;
        }
    }

    if (previewPath.isEmpty() || !QFileInfo::exists(previewPath))
    {
        restoreLastPreviewImage();
        updatePreviewDetails(file);
        return;
    }

    if (preview.isImage(file.extension))
    {
        QPixmap pixmap =
            preview.createImagePreview(
                previewPath,
                previewLabel->size()
            );

        if (!pixmap.isNull())
        {
            lastPreviewPixmap = pixmap;
            lastPreviewPath = previewPath;
            hasPersistentPreview = true;

            previewLabel->clear();
            previewLabel->setPixmap(
                lastPreviewPixmap.scaled(
                    previewLabel->size(),
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation
                )
            );
        }
        else
        {
            restoreLastPreviewImage();
        }

        updatePreviewDetails(file);
        return;
    }

    if (preview.isVideo(file.extension))
    {
        hasPersistentPreview = false;

        previewLabel->hide();

        if (videoWidget)
            videoWidget->show();

        if (mediaPlayer)
        {
            mediaPlayer->setSource(
                QUrl::fromLocalFile(previewPath)
            );

            mediaPlayer->play();
        }

        updatePreviewDetails(file);
        return;
    }

    restoreLastPreviewImage();
    updatePreviewDetails(file);
}

void MainWindow::clearPreviewDetails()
{
    currentPreviewFileIndex = -1;

    lastPreviewPixmap = QPixmap();
    lastPreviewPath.clear();
    hasPersistentPreview = false;

    if (mediaPlayer)
        mediaPlayer->stop();

    if (videoWidget)
        videoWidget->hide();

    if (previewLabel)
    {
        previewLabel->clear();
        previewLabel->show();
        previewLabel->setAlignment(Qt::AlignCenter);
        previewLabel->setText("Vista previa");
    }

    if (previewStatusLabel)
        previewStatusLabel->clear();

    if (previewNameLabel)
        previewNameLabel->setText("Nombre: -");

    if (previewTypeLabel)
        previewTypeLabel->setText("Tipo: -");

    if (previewSizeLabel)
        previewSizeLabel->setText("Tamaño: -");

    if (previewQualityLabel)
        previewQualityLabel->setText("Estado: -");

    if (previewOriginalPathLabel)
        previewOriginalPathLabel->setText("Ruta original: -");

    if (previewTempPathLabel)
        previewTempPathLabel->setText("Ruta temporal: -");

    if (openTempFileButton)
        openTempFileButton->setEnabled(false);

    if (openTempFolderButton)
        openTempFolderButton->setEnabled(false);
}

void MainWindow::updatePreviewDetails(
    const RecoverableFile& file
)
{
    previewNameLabel->setText("Nombre: " + file.originalName);
    previewTypeLabel->setText("Tipo: " + FileTypeHelper::typeName(file.extension));
    previewSizeLabel->setText("Tamaño: " + formatSize(file.size));
    previewQualityLabel->setText("Estado: " + FileTypeHelper::recoveryQualityText(file));
    previewOriginalPathLabel->setText("Ruta original: " + file.originalPath);
    previewTempPathLabel->setText("Ruta temporal: " + file.currentPath);

    QFileInfo info(file.currentPath);
    bool canOpen = info.exists() && info.isFile();

    openTempFileButton->setEnabled(canOpen);
    openTempFolderButton->setEnabled(canOpen);
}

void MainWindow::openCurrentTempFile()
{
    if (currentPreviewFileIndex < 0 || currentPreviewFileIndex >= files.size())
        return;

    const RecoverableFile& file = files[currentPreviewFileIndex];

    if (file.currentPath.isEmpty())
        return;

    QDesktopServices::openUrl(
        QUrl::fromLocalFile(file.currentPath)
    );
}

void MainWindow::openCurrentTempFolder()
{
    if (currentPreviewFileIndex < 0 || currentPreviewFileIndex >= files.size())
        return;

    const RecoverableFile& file = files[currentPreviewFileIndex];

    QFileInfo info(file.currentPath);

    if (!info.exists())
        return;

    QDesktopServices::openUrl(
        QUrl::fromLocalFile(info.absolutePath())
    );
}

QVector<RecoverableFile> MainWindow::selectedFiles() const
{
    QVector<RecoverableFile> selected;
    QSet<int> selectedIndexes;

    for (int row = 0; row < table->rowCount(); ++row)
    {
        QTableWidgetItem* checkItem = table->item(row, 0);

        if (!checkItem || checkItem->checkState() != Qt::Checked)
            continue;

        QTableWidgetItem* item = table->item(row, 1);

        if (!item)
            continue;

        const int fileIndex = item->data(Qt::UserRole).toInt();

        if (fileIndex >= 0 && fileIndex < files.size())
            selectedIndexes.insert(fileIndex);
    }

    if (gallery)
    {
        for (int i = 0; i < gallery->count(); ++i)
        {
            QListWidgetItem* item = gallery->item(i);

            if (!item || item->checkState() != Qt::Checked)
                continue;

            const int fileIndex = item->data(Qt::UserRole).toInt();

            if (fileIndex >= 0 && fileIndex < files.size())
                selectedIndexes.insert(fileIndex);
        }
    }

    for (int fileIndex : selectedIndexes)
    {
        selected.push_back(files[fileIndex]);
    }

    return selected;
}

QString MainWindow::formatSize(qint64 size) const
{
    double value = static_cast<double>(size);

    if (value < 1024)
        return QString::number(value, 'f', 0) + " B";

    value /= 1024;

    if (value < 1024)
        return QString::number(value, 'f', 1) + " KB";

    value /= 1024;

    if (value < 1024)
        return QString::number(value, 'f', 1) + " MB";

    value /= 1024;

    return QString::number(value, 'f', 1) + " GB";
}

QString MainWindow::sourceToText(
    RecoverySource source
) const
{
    switch (source)
    {
        case RecoverySource::RecycleBin:
            return "Papelera";

        case RecoverySource::Folder:
            return "Carpeta";

        case RecoverySource::DeepScan:
            return "Escaneo profundo";
    }

    return "Desconocido";
}

bool MainWindow::isUnsafeRecoveryDestination(
    const QString& destinationFolder
) const
{
    if (activeScanRoot.isEmpty())
        return false;

    QString destination =
        QDir::fromNativeSeparators(
            QFileInfo(destinationFolder).absoluteFilePath()
        ).toLower();

    QString scanRoot =
        QDir::fromNativeSeparators(
            QFileInfo(activeScanRoot).absoluteFilePath()
        ).toLower();

    if (scanRoot.endsWith("/"))
        scanRoot.chop(1);

    if (destination.endsWith("/"))
        destination.chop(1);

    return destination.startsWith(scanRoot);
}
void MainWindow::flushPendingScanFiles()
{
    const int maxPerTick = 300;
    const int maxVisibleDuringScan = 3000;

    if (!pendingScanFiles.isEmpty())
    {
        const int count =
            qMin(maxPerTick, pendingScanFiles.size());

        table->blockSignals(true);
        table->setUpdatesEnabled(false);
        table->setSortingEnabled(false);

        for (int i = 0; i < count; ++i)
        {
            const RecoverableFile file =
                pendingScanFiles.takeFirst();

            const int fileIndex = files.size();

            files.append(file);

            if (matchesCurrentFilters(file))
            {
                visibleFileIndexes.append(fileIndex);

                if (table->rowCount() < maxVisibleDuringScan)
                {
                    appendFileToTable(file, fileIndex);
                    appendFileToGallery(file, fileIndex);
                }
            }
        }

        table->setUpdatesEnabled(true);
        table->blockSignals(false);

        foundCountLabel->setText(
            QString("Encontrados: %1").arg(files.size())
        );

        resultCountLabel->setText(
            QString("Resultados visibles: %1 / cargados: %2")
                .arg(visibleFileIndexes.size())
                .arg(table->rowCount())
        );

        updateSummary();

        return;
    }

    if (!scanCompletionPending)
    {
        if (scanFlushTimer)
            scanFlushTimer->stop();

        return;
    }

    if (scanFlushTimer)
        scanFlushTimer->stop();

    scanCompletionPending = false;

    applyFilters();
    updateResultsTree();
    updateSummary();
    updateSelectedCount();

    if (previewStatusLabel)
    {
        previewStatusLabel->setText(
            QString("Escaneo cancelado. Archivos encontrados: %1")
                .arg(files.size())
        );
    }
    else
    {
        progressBar->setValue(100);
        scanStatusLabel->setText("Estado: Finalizado");

        previewLabel->setText(
            QString("Escaneo finalizado. Archivos encontrados: %1")
                .arg(files.size())
        );
    }

    foundCountLabel->setText(
        QString("Encontrados: %1").arg(files.size())
    );

    resultCountLabel->setText(
        QString("Resultados visibles: %1 / cargados: %2")
            .arg(visibleFileIndexes.size())
            .arg(table->rowCount())
    );

    setScanningState(false);

    if (scanThread)
        scanThread->quit();
}

void MainWindow::onTreeItemClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);

    if (!item) {
        return;
    }

    currentTreeCategoryKey.clear();
    currentTreeExtension.clear();

    QVariant categoryData = item->data(0, Qt::UserRole);
    QVariant extensionData = item->data(0, Qt::UserRole + 1);

    if (categoryData.isValid()) {
        currentTreeCategoryKey = categoryData.toString();
    }

    if (extensionData.isValid()) {
        currentTreeExtension = extensionData.toString();
    }

    if (currentTreeCategoryKey.isEmpty()) {
        currentTreeCategoryKey = "all";
    }

    applyFilters();
}

void MainWindow::loadMoreVisibleRows()
{
    const int maxRowsToLoad = 300;

    int loaded = 0;

    table->setUpdatesEnabled(false);
    table->blockSignals(true);

    if (gallery)
    {
        gallery->setUpdatesEnabled(false);
        gallery->blockSignals(true);
    }

    while (
        loadedVisibleRows < visibleFileIndexes.size() &&
        loaded < maxRowsToLoad
    )
    {
        const int fileIndex =
            visibleFileIndexes[loadedVisibleRows];

        if (fileIndex >= 0 && fileIndex < files.size())
        {
            appendFileToTable(
                files[fileIndex],
                fileIndex
            );

            appendFileToGallery(
                files[fileIndex],
                fileIndex
            );
        }

        ++loadedVisibleRows;
        ++loaded;
    }

    table->blockSignals(false);
    table->setUpdatesEnabled(true);

    if (gallery)
    {
        gallery->blockSignals(false);
        gallery->setUpdatesEnabled(true);
    }

    resultCountLabel->setText(
        QString("Resultados visibles: %1 / cargados: %2")
            .arg(visibleFileIndexes.size())
            .arg(table->rowCount())
    );
}

void MainWindow::restoreLastPreviewImage()
{
    if (!hasPersistentPreview || lastPreviewPixmap.isNull())
        return;

    if (videoWidget)
        videoWidget->hide();

    if (previewLabel)
    {
        previewLabel->show();
        previewLabel->clear();
        previewLabel->setAlignment(Qt::AlignCenter);
        previewLabel->setPixmap(
            lastPreviewPixmap.scaled(
                previewLabel->size(),
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation
            )
        );
    }
}

void MainWindow::setScanningState(bool scanning)
{
    deepScanButton->setEnabled(!scanning);
    scanRecycleBinButton->setEnabled(!scanning);
    scanFolderButton->setEnabled(!scanning);
    recoverButton->setEnabled(!scanning);
    cancelScanButton->setEnabled(scanning);
    driveCombo->setEnabled(!scanning);

    searchInput->setEnabled(true);
    resultsTree->setEnabled(true);
    selectAllButton->setEnabled(!scanning || table->rowCount() > 0);

    if (treeUpdateTimer)
    {
        if (scanning)
        {
            if (!treeUpdateTimer->isActive())
                treeUpdateTimer->start();
        }
        else
        {
            treeUpdateTimer->stop();
            updateResultsTree();
        }
    }
}