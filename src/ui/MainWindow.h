#pragma once

#include <QMainWindow>
#include <QWidget>
#include <QStackedWidget>
#include <QSet>

#include <QTableWidget>
#include <QTreeWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QProgressBar>

#include <QVector>
#include <QString>
#include <QTimer>
#include <QThread>
#include <QPixmap>
#include <QIcon>
#include <QImage>
#include <QHash>

#include <QMediaPlayer>
#include <QVideoWidget>
#include <QAudioOutput>

#include "models/RecoverableFile.h"

class ScanWorker;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onTreeItemClicked(QTreeWidgetItem* item, int column);
    void onVideoThumbnailReady(int fileIndex, const QImage& image);
    void onImageThumbnailReady(int fileIndex, const QImage& image);

private:
    QStackedWidget* pages = nullptr;

    QWidget* homePage = nullptr;
    QWidget* resultsPage = nullptr;
    QWidget* sidebar = nullptr;

    QPushButton* homeButton = nullptr;
    QPushButton* resultsButton = nullptr;

    QPushButton* recycleBinCard = nullptr;
    QPushButton* folderCard = nullptr;
    QPushButton* deepScanCard = nullptr;

    QComboBox* driveCombo = nullptr;

    QTreeWidget* resultsTree = nullptr;
    QLineEdit* searchInput = nullptr;
    QTimer* thumbnailTimer = nullptr;

    QHash<QString, QIcon> thumbnailCache;
    QSet<int> thumbnailPendingIndexes;

    class VideoThumbnailer* videoThumbnailer = nullptr;
    QSet<int> videoThumbnailRequested;
    QSet<int> imageThumbnailRequested;

    // Primer fotograma (tamaño completo) de cada video, por índice de archivo.
    // Se usa tanto en la miniatura como en la vista previa (sin reproducir).
    QHash<int, QImage> videoFrameCache;

    // Miniaturas ya generadas, por índice de archivo. Persisten al cambiar de
    // categoría en el árbol (la galería se reconstruye pero se reaplican).
    QHash<int, QIcon> videoIconCache;
    QHash<int, QIcon> imageIconCache;

    QIcon makeVideoThumbnailIcon(const QImage& frame) const;

    void setupThumbnailLoader();
    void requestVisibleThumbnails();
    void loadNextThumbnailBatch();
    QIcon placeholderIconForFile(const RecoverableFile& file) const;
    QIcon realThumbnailForFile(RecoverableFile& file, int fileIndex = -1);
    /*
     * Vista de resultados:
     * - resultsViews permite cambiar entre lista y miniaturas.
     * - table mantiene tu implementación actual.
     * - gallery muestra los archivos como Windows, con miniaturas.
     */
    QStackedWidget* resultsViews = nullptr;
    QComboBox* viewModeCombo = nullptr;

    QTableWidget* table = nullptr;
    QListWidget* gallery = nullptr;

    QLabel* previewLabel = nullptr;
    QLabel* previewStatusLabel = nullptr;
    QLabel* previewNameLabel = nullptr;
    QLabel* previewTypeLabel = nullptr;
    QLabel* previewSizeLabel = nullptr;
    QLabel* previewQualityLabel = nullptr;
    QLabel* previewOriginalPathLabel = nullptr;
    QLabel* previewTempPathLabel = nullptr;

    QVideoWidget* videoWidget = nullptr;
    QMediaPlayer* mediaPlayer = nullptr;
    QAudioOutput* audioOutput = nullptr;

    QPixmap lastPreviewPixmap;
    QString lastPreviewPath;
    bool hasPersistentPreview = false;

    QPushButton* openTempFileButton = nullptr;
    QPushButton* openTempFolderButton = nullptr;

    QLabel* resultCountLabel = nullptr;
    QLabel* scanStatusLabel = nullptr;
    QLabel* scanDriveLabel = nullptr;
    QLabel* foundCountLabel = nullptr;
    QLabel* selectedCountLabel = nullptr;

    QProgressBar* progressBar = nullptr;

    QPushButton* scanRecycleBinButton = nullptr;
    QPushButton* scanFolderButton = nullptr;
    QPushButton* deepScanButton = nullptr;
    QPushButton* cancelScanButton = nullptr;
    QPushButton* recoverButton = nullptr;
    QPushButton* selectAllButton = nullptr;

    QVector<RecoverableFile> files;
    QVector<RecoverableFile> pendingScanFiles;
    QVector<int> visibleFileIndexes;

    QString activeScanRoot;
    QString currentTreeCategoryKey = "all";
    QString currentTreeExtension;
    QString currentTreeScope;   // "" = todo, "reconstructed", "original"

    int currentPreviewFileIndex = -1;

    int loadedVisibleRows = 0;
    int rowsPerBatch = 200;

    QThread* scanThread = nullptr;
    ScanWorker* scanWorker = nullptr;

    QTimer* scanFlushTimer = nullptr;

    bool scanCompletionPending = false;
    bool scanWasCancelled = false;

    // Categorías de archivo seleccionadas en el diálogo previo al escaneo.
    // Si está vacío se muestran todas.
    QSet<QString> m_selectedFileCategories;

    QTimer* treeUpdateTimer = nullptr;

    QTreeWidgetItem* allTreeItem = nullptr;
    QTreeWidgetItem* imagesTreeItem = nullptr;
    QTreeWidgetItem* videosTreeItem = nullptr;
    QTreeWidgetItem* documentsTreeItem = nullptr;
    QTreeWidgetItem* audioTreeItem = nullptr;
    QTreeWidgetItem* compressedTreeItem = nullptr;
    QTreeWidgetItem* othersTreeItem = nullptr;

private:
    void setupUi();

    QWidget* createSidebar();
    QWidget* createHomePage();
    QWidget* createResultsPage();

    QPushButton* createSidebarButton(const QString& text);

    QPushButton* createScanCard(
        const QString& title,
        const QString& subtitle,
        const QString& icon
    );

    void setupResultsTree();
    void updateResultsTree();
    void updateResultsTreeCounts();
    void loadDrives();

    void scanRecycleBin();
    void scanFolder();
    void deepScan();
    void cancelDeepScan();

    void recoverSelectedFiles();

    void fillTable();

    void appendFileToTable(
        const RecoverableFile& file,
        int fileIndex
    );

    void appendFilesToTable(
        const QVector<RecoverableFile>& newFiles
    );

    /*
     * Nuevas funciones para vista de miniaturas.
     */
    void appendFileToGallery(
        const RecoverableFile& file,
        int fileIndex
    );

    QIcon iconForRecoverableFile(
        const RecoverableFile& file
    ) const;

    void fillGallery();

    void flushPendingScanFiles();

    void loadMoreVisibleRows();

    void applyFilters();

    bool matchesCurrentFilters(
        const RecoverableFile& file
    ) const;

    void updateSummary();
    void updateSelectedCount();
    void toggleSelectAll();

    void showPreview(int row);
    void showPreviewByFileIndex(int fileIndex);

    void clearPreviewDetails();

    void updatePreviewDetails(
        const RecoverableFile& file
    );

    void openCurrentTempFile();
    void openCurrentTempFolder();

    QVector<RecoverableFile> selectedFiles() const;

    QString formatSize(qint64 size) const;

    QString sourceToText(
        RecoverySource source
    ) const;

    bool isUnsafeRecoveryDestination(
        const QString& destinationFolder
    ) const;

    void setScanningState(bool scanning);

    void restoreLastPreviewImage();

    QString categoryKeyForFile(const RecoverableFile& file) const;
    QString categoryTextFromKey(const QString& key) const;
};