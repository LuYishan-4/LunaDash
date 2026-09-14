#include <LuDash/file_picker/FilePicker.h>

#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QStandardPaths>
#include <QToolButton>
#include <QVBoxLayout>

namespace LuDash {

QSize boundedPreviewSize(const QSize &sourceSize, const QSize &bounds) {
  if (!sourceSize.isValid() || sourceSize.isEmpty() || !bounds.isValid() ||
      bounds.isEmpty())
    return {};
  if (sourceSize.width() <= bounds.width() &&
      sourceSize.height() <= bounds.height())
    return sourceSize;
  return sourceSize.scaled(bounds, Qt::KeepAspectRatio);
}

bool isEligibleImageFile(const QString &path) {
  const QFileInfo info(path);
  if (!info.exists() || !info.isFile() || info.isSymLink() ||
      info.size() <= 0 || info.size() > maximumImageFileBytes)
    return false;

  const QString suffix = info.suffix().toLower();
  if (suffix != "png" && suffix != "jpg" && suffix != "jpeg" &&
      suffix != "webp")
    return false;

  QImageReader reader(info.absoluteFilePath());
  const QSize size = reader.size();
  if (!reader.canRead() || !size.isValid() || size.isEmpty())
    return false;
  return qint64(size.width()) * qint64(size.height()) <= maximumImagePixels;
}

namespace {
class ImageSelectionDialog final : public QDialog {
public:
  ImageSelectionDialog(QWidget *parent, const QString &initialPath)
      : QDialog(parent) {
    setWindowTitle("Choose image");
    resize(900, 620);

    auto *layout = new QVBoxLayout(this);
    auto *navigation = new QHBoxLayout;
    auto *up = new QToolButton;
    up->setText("Up");
    pathEdit_ = new QLineEdit;
    pathEdit_->setPlaceholderText("Image folder path");
    auto *viewButton = new QToolButton;
    viewButton->setText("Grid");
    viewButton->setCheckable(true);
    navigation->addWidget(up);
    navigation->addWidget(pathEdit_, 1);
    navigation->addWidget(viewButton);
    layout->addLayout(navigation);

    auto *content = new QHBoxLayout;
    model_ = new QFileSystemModel(this);
    model_->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot |
                      QDir::Readable);
    model_->setNameFilters({"*.png", "*.jpg", "*.jpeg", "*.webp", "*.PNG",
                            "*.JPG", "*.JPEG", "*.WEBP"});
    model_->setNameFilterDisables(false);
    view_ = new QListView;
    view_->setModel(model_);
    view_->setSelectionMode(QAbstractItemView::SingleSelection);
    content->addWidget(view_, 2);

    auto *detailsLayout = new QVBoxLayout;
    preview_ = new QLabel("Select an image to preview");
    preview_->setAlignment(Qt::AlignCenter);
    preview_->setMinimumSize(280, 280);
    preview_->setWordWrap(true);
    fileName_ = new QLabel;
    fileName_->setWordWrap(true);
    filePath_ = new QLabel;
    filePath_->setWordWrap(true);
    detailsLayout->addWidget(preview_, 1);
    detailsLayout->addWidget(fileName_);
    detailsLayout->addWidget(filePath_);
    content->addLayout(detailsLayout, 1);
    layout->addLayout(content, 1);

    buttons_ =
        new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel);
    buttons_->button(QDialogButtonBox::Open)->setEnabled(false);
    layout->addWidget(buttons_);

    connect(up, &QToolButton::clicked, this, [this] {
      navigate(QDir(currentDirectory_).absoluteFilePath(".."));
    });
    connect(pathEdit_, &QLineEdit::returnPressed, this,
            [this] { navigate(pathEdit_->text()); });
    connect(
        viewButton, &QToolButton::toggled, this, [this, viewButton](bool grid) {
          view_->setViewMode(grid ? QListView::IconMode : QListView::ListMode);
          view_->setIconSize(grid ? QSize(96, 96) : QSize(24, 24));
          view_->setGridSize(grid ? QSize(130, 125) : QSize());
          viewButton->setText(grid ? "List" : "Grid");
        });
    connect(view_->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex &index) { updateSelection(index); });
    connect(view_, &QListView::doubleClicked, this,
            [this](const QModelIndex &index) {
              const QFileInfo info = model_->fileInfo(index);
              if (info.isDir())
                navigate(info.absoluteFilePath());
              else if (isEligibleImageFile(info.absoluteFilePath()))
                accept();
            });
    connect(buttons_, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QFileInfo initial(initialPath);
    QString directory;
    QString selectedFile;
    if (initial.isFile()) {
      directory = initial.absolutePath();
      selectedFile = initial.absoluteFilePath();
    } else if (initial.isDir()) {
      directory = initial.absoluteFilePath();
    } else {
      directory =
          QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
      if (directory.isEmpty() || !QFileInfo(directory).isDir())
        directory = QDir::homePath();
    }
    navigate(directory);
    if (!selectedFile.isEmpty()) {
      const QModelIndex index = model_->index(selectedFile);
      view_->setCurrentIndex(index);
      view_->scrollTo(index);
    }
  }

  QString selectedPath() const { return selectedPath_; }

private:
  void navigate(const QString &path) {
    const QFileInfo info(path);
    if (!info.isDir()) {
      pathEdit_->setText(currentDirectory_);
      return;
    }
    currentDirectory_ = info.absoluteFilePath();
    pathEdit_->setText(currentDirectory_);
    view_->setRootIndex(model_->setRootPath(currentDirectory_));
    view_->clearSelection();
    selectedPath_.clear();
    updatePreview({});
  }

  void updateSelection(const QModelIndex &index) {
    const QFileInfo info = model_->fileInfo(index);
    selectedPath_ = isEligibleImageFile(info.absoluteFilePath())
                        ? info.absoluteFilePath()
                        : QString{};
    buttons_->button(QDialogButtonBox::Open)
        ->setEnabled(!selectedPath_.isEmpty());
    updatePreview(selectedPath_);
  }

  void updatePreview(const QString &path) {
    preview_->setPixmap({});
    if (path.isEmpty()) {
      preview_->setText("Select a PNG, JPEG, or WebP image to preview");
      fileName_->clear();
      filePath_->clear();
      return;
    }

    const QFileInfo info(path);
    fileName_->setText(QString("Name: %1").arg(info.fileName()));
    filePath_->setText(QString("Path: %1\nSize: %2 × %3")
                           .arg(info.absoluteFilePath())
                           .arg(imageSize(path).width())
                           .arg(imageSize(path).height()));
    QImageReader reader(path);
    const QSize target = boundedPreviewSize(reader.size(), {512, 512});
    if (target.isEmpty()) {
      preview_->setText("Preview unavailable");
      return;
    }
    reader.setScaledSize(target);
    const QImage image = reader.read();
    if (image.isNull())
      preview_->setText("Preview unavailable");
    else {
      preview_->setText({});
      preview_->setPixmap(QPixmap::fromImage(image));
    }
  }

  static QSize imageSize(const QString &path) {
    return QImageReader(path).size();
  }

  QFileSystemModel *model_ = nullptr;
  QListView *view_ = nullptr;
  QLineEdit *pathEdit_ = nullptr;
  QLabel *preview_ = nullptr;
  QLabel *fileName_ = nullptr;
  QLabel *filePath_ = nullptr;
  QDialogButtonBox *buttons_ = nullptr;
  QString currentDirectory_;
  QString selectedPath_;
};
} // namespace

QString selectImageFile(QWidget *parent, const QString &initialPath) {
  ImageSelectionDialog dialog(parent, initialPath);
  return dialog.exec() == QDialog::Accepted ? dialog.selectedPath() : QString{};
}

} // namespace LuDash
