#pragma once
#include <QDialog>
#include <QStringList>
#include <QVariantMap>

class QFileSystemModel;
class QTreeView;
class QListView;
class QStackedWidget;
class QLineEdit;
class QLabel;
class QPushButton;
class QComboBox;
class QCheckBox;
class QModelIndex;
class QVBoxLayout;

namespace LunaDash {
class FilePickerFilterModel;
// One owning dialog: selection data survives accept() until the caller reads
// it.
class FilePickerDialog final : public QDialog {
public:
  enum class Mode { Open, Save, Directory };
  FilePickerDialog(Mode mode, const QString &title, const QVariantMap &options,
                   QWidget *parent = nullptr);
  QStringList selectedPaths() const;
  int selectedFilter() const;
  void setFilters(const QList<QPair<QString, QStringList>> &filters,
                  int current);
  void addChoice(const QString &id, const QString &label,
                 const QList<QPair<QString, QString>> &values,
                 const QString &selected);
  QList<QPair<QString, QString>> selectedChoices() const;

protected:
  void accept() override;

private:
  void navigate(const QString &path, bool remember = true);
  void applyLocation();
  void activate(const QModelIndex &index);
  void updateSelection();
  void updatePreview(const QString &path);
  void updateFilters();
  void updateStatus();
  void showError(const QString &message);
  QString resolvePath(const QString &text) const;
  QStringList candidatePaths() const;

  Mode mode_;
  bool multiple_;
  QString folder_;
  QStringList history_;
  int historyIndex_ = -1;
  QStringList acceptedPaths_;
  QList<QStringList> patterns_;
  QFileSystemModel *model_ = nullptr;
  FilePickerFilterModel *proxy_ = nullptr;
  QTreeView *details_ = nullptr;
  QListView *icons_ = nullptr;
  QStackedWidget *views_ = nullptr;
  QLineEdit *location_ = nullptr;
  QLineEdit *search_ = nullptr;
  QLineEdit *filename_ = nullptr;
  QComboBox *filter_ = nullptr;
  QCheckBox *hidden_ = nullptr;
  QLabel *status_ = nullptr;
  QLabel *error_ = nullptr;
  QLabel *previewImage_ = nullptr;
  QLabel *previewName_ = nullptr;
  QLabel *previewMeta_ = nullptr;
  QPushButton *back_ = nullptr;
  QPushButton *forward_ = nullptr;
  QPushButton *accept_ = nullptr;
  QVBoxLayout *choices_ = nullptr;
  QList<QPair<QString, QComboBox *>> choiceFields_;
};
} // namespace LunaDash
