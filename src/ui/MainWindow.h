#pragma once

#include <QMainWindow>
#include <QWidget>
#include <QStackedWidget>

#include <QTableWidget>
#include <QTreeWidget>
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
    QTableWidget* table = nullptr;

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

    int currentPreviewFileIndex = -1;

    int loadedVisibleRows = 0;
    int rowsPerBatch = 200;

    QThread* scanThread = nullptr;
    ScanWorker* scanWorker = nullptr;

    QTimer* scanFlushTimer = nullptr;

    bool scanCompletionPending = false;
    bool scanWasCancelled = false;

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