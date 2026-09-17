#pragma once
#include <QObject>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QHash>
#include <algorithm>
class QWidget;
class QFileSystemModel;
class QAbstractItemView;
class QTreeView;
class QListView;
class QLineEdit;
class QLabel;
class QListWidget;
class QPushButton;
class QAction;
class QMimeData;
namespace LuDash {
class FileManagerActions final : public QObject {
public:
    explicit FileManagerActions(QWidget* page);
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
private:
    QAbstractItemView* activeView() const;
    QStringList selection() const;
    void navigate(const QString& path, bool remember = true);
    void installModel();
    void refreshPlaces();
    void updateActions();
    void openSelection(bool chooseApplication = false);
    void contextMenu(QAbstractItemView* view, const QPoint& position);
    void copySelection(bool cut);
    void paste(const QString& destination);
    void run(const QString& operation, const QStringList& paths, const QString& destination,
             bool clearClipboard = false);
    void createEntry(bool folder, const QString& destination);
    void renameSelection();
    void trashSelection();
    void openTerminal(const QString& directory);
    void properties();
    void sort(int column, bool ascending);
    QStringList clipboardFiles() const;
    QWidget* page_;
    QFileSystemModel* model_ = nullptr;
    QTreeView* details_;
    QListView* icons_;
    QLineEdit* location_;
    QLineEdit* search_;
    QLabel* status_;
    QLabel* folderTitle_;
    QListWidget* places_;
    QPushButton* mode_;
    QPushButton* hidden_;
    QPushButton* back_;
    QPushButton* forward_;
    QHash<QString, QAction*> actions_;
    QString currentPath_;
    QStringList history_;
    int historyIndex_ = -1;
    int sortColumn_ = 0;
    bool ascending_ = true;
    bool busy_ = false;
};
} // namespace LuDash
