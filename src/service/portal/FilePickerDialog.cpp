#include "service/portal/FilePickerDialog.hpp"
#include "config/localization/Localization.hpp"
#include "service/portal/FileIcons.hpp"
#include "desktop/theme/DesktopTheme.hpp"
#include <QtWidgets>
#include <utility>

namespace LunaDash {
class FilePickerFilterModel final : public QSortFilterProxyModel {
public:
  explicit FilePickerFilterModel(QObject *parent)
      : QSortFilterProxyModel(parent) {}
  void apply(QString search, QStringList patterns) {
    search_ = std::move(search);
    patterns_ = std::move(patterns);
    invalidateFilter();
  }

protected:
  bool filterAcceptsRow(int row, const QModelIndex &parent) const override {
    const auto *files = static_cast<QFileSystemModel *>(sourceModel());
    const auto index = files->index(row, 0, parent);
    if (files->isDir(index))
      return true;
    const auto name = files->fileName(index);
    return name.contains(search_, Qt::CaseInsensitive) &&
           (patterns_.isEmpty() || QDir::match(patterns_, name));
  }
  bool lessThan(const QModelIndex &left,
                const QModelIndex &right) const override {
    const auto *files = static_cast<QFileSystemModel *>(sourceModel());
    if (files->isDir(left) != files->isDir(right))
      return sortOrder() == Qt::AscendingOrder ? files->isDir(left)
                                               : !files->isDir(left);
    return QSortFilterProxyModel::lessThan(left, right);
  }

private:
  QString search_;
  QStringList patterns_;
};

namespace {
class PickerPathField final : public QLineEdit {
public:
  explicit PickerPathField(QWidget *parent) : QLineEdit(parent) {}

protected:
  void keyPressEvent(QKeyEvent *event) override {
    QLineEdit::keyPressEvent(event);
    // Navigation/search Enter must not reach the dialog's default button.
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
      event->accept();
  }
};

class PickerHeader final : public QFrame {
public:
  explicit PickerHeader(QWidget *parent) : QFrame(parent) {}

protected:
  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton && window()->windowHandle())
      window()->windowHandle()->startSystemMove();
    QFrame::mousePressEvent(event);
  }
  void mouseDoubleClickEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton) {
      if (window()->isMaximized())
        window()->showNormal();
      else
        window()->showMaximized();
    }
  }
};

QString optionPath(const QVariantMap &options, const char *key) {
  auto bytes = options.value(QLatin1String(key)).toByteArray();
  if (bytes.endsWith('\0'))
    bytes.chop(1);
  return QFile::decodeName(bytes);
}
} // namespace

FilePickerDialog::FilePickerDialog(Mode mode, const QString &title,
                                   const QVariantMap &options, QWidget *parent)
    : QDialog(parent), mode_(mode),
      multiple_(mode != Mode::Save && options.value("multiple").toBool()) {
  setObjectName("portalFileShell");
  setWindowTitle(title);
  setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
  setAttribute(Qt::WA_TranslucentBackground);
  setModal(options.value("modal", true).toBool());
  setMinimumSize(720, 480);
  resize(1120, 740);
  if (const auto *screen = QGuiApplication::primaryScreen())
    resize(
        size().boundedTo(screen->availableGeometry().size() - QSize(32, 32)));
  watchDesktopTheme(this);

  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  auto *frame = new QFrame(this);
  frame->setObjectName("portalFileFrame");
  outer->addWidget(frame);
  auto *layout = new QVBoxLayout(frame);
  layout->setContentsMargins(20, 18, 20, 12);
  layout->setSpacing(14);

  auto *header = new PickerHeader(frame);
  header->setObjectName("portalFileHeader");
  auto *headerLayout = new QHBoxLayout(header);
  headerLayout->setContentsMargins(18, 12, 12, 12);
  auto *brand = new QLabel(header);
  brand->setPixmap(fileIcon(FileIcon::Folder).pixmap(30, 30));
  headerLayout->addWidget(brand);
  auto *titles = new QVBoxLayout;
  auto *heading = new QLabel(title, header);
  heading->setTextFormat(Qt::PlainText);
  heading->setObjectName("portalFileHeading");
  auto *caption =
      new QLabel(translate("Browse, preview and choose your files"), header);
  caption->setObjectName("portalFileCaption");
  titles->addWidget(heading);
  titles->addWidget(caption);
  headerLayout->addLayout(titles, 1);
  auto *maximize = new QToolButton(header);
  maximize->setText(QStringLiteral("□"));
  maximize->setToolTip(translate("Maximize or restore"));
  connect(maximize, &QToolButton::clicked, this, [this] {
    if (isMaximized())
      showNormal();
    else
      showMaximized();
  });
  auto *close = new QToolButton(header);
  close->setObjectName("portalFileClose");
  close->setText(QStringLiteral("×"));
  close->setToolTip(translate("Cancel"));
  connect(close, &QToolButton::clicked, this, &QDialog::reject);
  headerLayout->addWidget(maximize);
  headerLayout->addWidget(close);
  layout->addWidget(header);

  auto tool = [frame](FileIcon icon, const QString &label) {
    auto *button = new QPushButton(fileIcon(icon), {}, frame);
    button->setProperty("fileTool", true);
    button->setAccessibleName(label);
    button->setToolTip(label);
    button->setFixedSize(38, 38);
    button->setIconSize({20, 20});
    button->setAutoDefault(false);
    return button;
  };
  auto *navigation = new QHBoxLayout;
  back_ = tool(FileIcon::Back, translate("Back"));
  forward_ = tool(FileIcon::Forward, translate("Forward"));
  auto *up = tool(FileIcon::Up, translate("Up"));
  navigation->addWidget(back_);
  navigation->addWidget(forward_);
  navigation->addWidget(up);
  location_ = new PickerPathField(frame);
  location_->setObjectName("portalLocation");
  location_->setAccessibleName(translate("Location"));
  location_->setPlaceholderText(
      translate("Paste an absolute path or file:// URL"));
  navigation->addWidget(location_, 1);
  auto *go = tool(FileIcon::Forward, translate("Go"));
  navigation->addWidget(go);
  search_ = new PickerPathField(frame);
  search_->setPlaceholderText(translate("Search this folder"));
  search_->setClearButtonEnabled(true);
  search_->setMaximumWidth(230);
  search_->addAction(fileIcon(FileIcon::Search), QLineEdit::LeadingPosition);
  navigation->addWidget(search_);
  layout->addLayout(navigation);

  auto *splitter = new QSplitter(frame);
  splitter->setChildrenCollapsible(false);
  auto *places = new QListWidget(splitter);
  places->setObjectName("filePlaces");
  places->setMinimumWidth(150);
  places->setMaximumWidth(195);
  places->setIconSize({20, 20});
  places->setSpacing(4);
  auto place = [places](const QString &path, const QString &name,
                        FileIcon icon) {
    if (path.isEmpty() || !QFileInfo(path).isDir())
      return;
    auto *item = new QListWidgetItem(fileIcon(icon), name, places);
    item->setData(Qt::UserRole, path);
    item->setToolTip(path);
  };
  place(QDir::homePath(), translate("Home"), FileIcon::Home);
  place(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
        translate("Desktop"), FileIcon::Desktop);
  place(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
        translate("Downloads"), FileIcon::Download);
  place(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        translate("Documents"), FileIcon::File);
  place(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        translate("Pictures"), FileIcon::Picture);
  place(QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
        translate("Music"), FileIcon::Music);
  place(QStandardPaths::writableLocation(QStandardPaths::MoviesLocation),
        translate("Videos"), FileIcon::Video);
  place("/", translate("File system"), FileIcon::Drive);
  for (const auto &volume : QStorageInfo::mountedVolumes()) {
    const auto path = volume.rootPath();
    if (volume.isValid() && volume.isReady() &&
        (path.startsWith("/media/") || path.startsWith("/run/media/") ||
         path.startsWith("/mnt/")))
      place(path, volume.displayName(), FileIcon::Drive);
  }
  connect(places, &QListWidget::itemClicked, this,
          [this](QListWidgetItem *item) {
            navigate(item->data(Qt::UserRole).toString());
          });

  auto *browser = new QWidget(splitter);
  auto *browserLayout = new QVBoxLayout(browser);
  browserLayout->setContentsMargins(0, 0, 0, 0);
  auto *tools = new QHBoxLayout;
  status_ = new QLabel(browser);
  status_->setObjectName("fileStatus");
  tools->addWidget(status_, 1);
  hidden_ = new QCheckBox(translate("Show hidden files"), browser);
  tools->addWidget(hidden_);
  auto *viewMode = tool(FileIcon::Grid, translate("Switch view"));
  viewMode->setCheckable(true);
  tools->addWidget(viewMode);
  browserLayout->addLayout(tools);

  model_ = new QFileSystemModel(this);
  model_->setReadOnly(true);
  model_->setOption(QFileSystemModel::DontUseCustomDirectoryIcons);
  proxy_ = new FilePickerFilterModel(this);
  proxy_->setSourceModel(model_);
  proxy_->setSortCaseSensitivity(Qt::CaseInsensitive);
  proxy_->setSortLocaleAware(true);
  views_ = new QStackedWidget(browser);
  details_ = new QTreeView(views_);
  details_->setObjectName("fileView");
  details_->setModel(proxy_);
  details_->setRootIsDecorated(false);
  details_->setItemsExpandable(false);
  details_->setSortingEnabled(true);
  details_->sortByColumn(0, Qt::AscendingOrder);
  details_->setSelectionBehavior(QAbstractItemView::SelectRows);
  details_->setUniformRowHeights(true);
  details_->setColumnWidth(0, 300);
  details_->setColumnWidth(1, 85);
  details_->hideColumn(2);
  icons_ = new QListView(views_);
  icons_->setObjectName("fileIcons");
  icons_->setModel(proxy_);
  icons_->setViewMode(QListView::IconMode);
  icons_->setResizeMode(QListView::Adjust);
  icons_->setMovement(QListView::Static);
  icons_->setIconSize({48, 48});
  icons_->setGridSize({130, 108});
  icons_->setWordWrap(true);
  icons_->setSelectionModel(details_->selectionModel());
  for (QAbstractItemView *view : {static_cast<QAbstractItemView *>(details_),
                                  static_cast<QAbstractItemView *>(icons_)}) {
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setSelectionMode(multiple_ ? QAbstractItemView::ExtendedSelection
                                     : QAbstractItemView::SingleSelection);
    connect(view, &QAbstractItemView::activated, this,
            &FilePickerDialog::activate);
  }
  views_->addWidget(details_);
  views_->addWidget(icons_);
  browserLayout->addWidget(views_, 1);
  connect(viewMode, &QPushButton::toggled, this,
          [this](bool grid) { views_->setCurrentIndex(grid ? 1 : 0); });
  connect(details_->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, &FilePickerDialog::updateSelection);

  auto *preview = new QFrame(splitter);
  preview->setObjectName("filePreviewPanel");
  preview->setMinimumWidth(170);
  preview->setMaximumWidth(240);
  auto *previewLayout = new QVBoxLayout(preview);
  previewLayout->setContentsMargins(14, 16, 14, 16);
  auto *previewLabel = new QLabel(translate("Preview"), preview);
  previewLabel->setObjectName("filePreviewEyebrow");
  previewLayout->addWidget(previewLabel);
  previewImage_ = new QLabel(preview);
  previewImage_->setObjectName("filePreviewImage");
  previewImage_->setFixedHeight(164);
  previewImage_->setAlignment(Qt::AlignCenter);
  previewLayout->addWidget(previewImage_);
  previewName_ = new QLabel(preview);
  previewName_->setObjectName("filePreviewTitle");
  previewName_->setTextFormat(Qt::PlainText);
  previewName_->setWordWrap(true);
  previewLayout->addWidget(previewName_);
  previewMeta_ = new QLabel(preview);
  previewMeta_->setObjectName("filePreviewMeta");
  previewMeta_->setTextFormat(Qt::PlainText);
  previewMeta_->setWordWrap(true);
  previewLayout->addWidget(previewMeta_);
  previewLayout->addStretch();
  auto *hint = new QLabel(translate("Double-click a folder to open it. Select "
                                    "a file, then confirm below."),
                          preview);
  hint->setObjectName("filePreviewHint");
  hint->setWordWrap(true);
  previewLayout->addWidget(hint);
  splitter->setStretchFactor(1, 1);
  layout->addWidget(splitter, 1);

  error_ = new QLabel(frame);
  error_->setObjectName("danger");
  error_->setTextFormat(Qt::PlainText);
  error_->setWordWrap(true);
  error_->hide();
  layout->addWidget(error_);
  choices_ = new QVBoxLayout;
  layout->addLayout(choices_);
  auto *selection = new QHBoxLayout;
  selection->addWidget(new QLabel(
      translate(mode == Mode::Directory ? "Folder" : "File name"), frame));
  filename_ = new QLineEdit(frame);
  filename_->setObjectName("portalFilename");
  filename_->setAccessibleName(translate("File name"));
  filename_->setPlaceholderText(
      translate(mode == Mode::Save ? "Enter a file name"
                                   : "Select a file or paste its path"));
  filename_->setVisible(mode != Mode::Directory);
  selection->addWidget(filename_, 1);
  filter_ = new QComboBox(frame);
  filter_->setAccessibleName(translate("File type"));
  filter_->setMaximumWidth(260);
  filter_->hide();
  selection->addWidget(filter_);
  layout->addLayout(selection);
  auto *footer = new QHBoxLayout;
  auto *keyboard =
      new QLabel(translate("Ctrl+L · Location    Ctrl+F · Search"), frame);
  keyboard->setObjectName("fileStatus");
  footer->addWidget(keyboard, 1);
  auto *cancel = new QPushButton(translate("Cancel"), frame);
  cancel->setAutoDefault(false);
  connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
  footer->addWidget(cancel);
  QString acceptLabel = options.value("accept_label").toString();
  if (acceptLabel.isEmpty())
    acceptLabel = translate(mode == Mode::Save        ? "Save"
                            : mode == Mode::Directory ? "Choose folder"
                                                      : "Open file");
  else
    acceptLabel.replace('&', "&&").replace('_', '&');
  accept_ = new QPushButton(acceptLabel, frame);
  accept_->setObjectName("accent");
  accept_->setDefault(true);
  accept_->setMinimumWidth(140);
  connect(accept_, &QPushButton::clicked, this, &FilePickerDialog::accept);
  footer->addWidget(accept_);
  footer->addWidget(new QSizeGrip(frame), 0, Qt::AlignBottom);
  layout->addLayout(footer);

  connect(location_, &QLineEdit::returnPressed, this,
          &FilePickerDialog::applyLocation);
  connect(go, &QPushButton::clicked, this, &FilePickerDialog::applyLocation);
  connect(up, &QPushButton::clicked, this,
          [this] { navigate(QFileInfo(folder_).dir().absolutePath()); });
  connect(back_, &QPushButton::clicked, this, [this] {
    if (historyIndex_ > 0)
      navigate(history_.at(--historyIndex_), false);
  });
  connect(forward_, &QPushButton::clicked, this, [this] {
    if (historyIndex_ + 1 < history_.size())
      navigate(history_.at(++historyIndex_), false);
  });
  connect(search_, &QLineEdit::textChanged, this,
          &FilePickerDialog::updateFilters);
  connect(filter_, &QComboBox::currentIndexChanged, this,
          &FilePickerDialog::updateFilters);
  connect(hidden_, &QCheckBox::toggled, this, &FilePickerDialog::updateFilters);
  connect(filename_, &QLineEdit::textEdited, this, [this] {
    const QSignalBlocker blocker(details_->selectionModel());
    details_->selectionModel()->clearSelection();
    updateStatus();
  });
  connect(model_, &QFileSystemModel::directoryLoaded, this,
          [this](const QString &path) {
            if (path == folder_)
              updateStatus();
          });
  auto shortcut = [this](const QKeySequence &key, auto action) {
    auto *binding = new QShortcut(key, this);
    connect(binding, &QShortcut::activated, this, action);
  };
  shortcut(QKeySequence("Ctrl+L"), [this] {
    location_->setFocus();
    location_->selectAll();
  });
  shortcut(QKeySequence::Find, [this] {
    search_->setFocus();
    search_->selectAll();
  });
  shortcut(QKeySequence("Alt+Left"), [this] { back_->click(); });
  shortcut(QKeySequence("Alt+Right"), [this] { forward_->click(); });
  shortcut(QKeySequence("Alt+Up"), [up] { up->click(); });
  shortcut(QKeySequence("Ctrl+H"), [this] { hidden_->toggle(); });

  QString initial = optionPath(options, "current_folder");
  const auto currentFile = optionPath(options, "current_file");
  if (initial.isEmpty() && !currentFile.isEmpty())
    initial = QFileInfo(currentFile).absolutePath();
  if (!QFileInfo(initial).isDir())
    initial = QDir::homePath();
  updateFilters();
  navigate(initial);
  if (mode == Mode::Save) {
    filename_->setText(
        options.value("current_name", QFileInfo(currentFile).fileName())
            .toString());
    filename_->setFocus();
    filename_->selectAll();
  }
  updateStatus();
}

QString FilePickerDialog::resolvePath(const QString &text) const {
  QString path = text;
  if (path.startsWith("file:", Qt::CaseInsensitive)) {
    const QUrl url(path);
    if (!url.isValid() || !url.isLocalFile() ||
        (!url.host().isEmpty() && url.host() != "localhost"))
      return {};
    path = url.toLocalFile();
  } else if (path.contains("://"))
    return {};
  if (path == "~")
    path = QDir::homePath();
  else if (path.startsWith("~/"))
    path = QDir::homePath() + path.mid(1);
  if (path.isEmpty() || path.contains(QChar::Null))
    return {};
  return QDir::cleanPath(QDir(folder_).absoluteFilePath(path));
}

void FilePickerDialog::navigate(const QString &value, bool remember) {
  const auto path = resolvePath(value);
  const QFileInfo directory(path);
  if (!directory.isDir() || !directory.isReadable()) {
    showError(translate("This folder does not exist or cannot be read."));
    return;
  }
  folder_ = path;
  if (remember && (historyIndex_ < 0 || history_.at(historyIndex_) != path)) {
    history_ = history_.mid(0, historyIndex_ + 1);
    history_.append(path);
    historyIndex_ = history_.size() - 1;
  }
  const QSignalBlocker blocker(details_->selectionModel());
  details_->selectionModel()->clear();
  if (mode_ != Mode::Save)
    filename_->clear();
  search_->clear();
  const auto root = proxy_->mapFromSource(model_->setRootPath(path));
  details_->setRootIndex(root);
  icons_->setRootIndex(root);
  location_->setText(path);
  back_->setEnabled(historyIndex_ > 0);
  forward_->setEnabled(historyIndex_ + 1 < history_.size());
  error_->hide();
  updatePreview(path);
  updateStatus();
}

void FilePickerDialog::applyLocation() {
  const auto path = resolvePath(location_->text());
  if (path.isEmpty()) {
    showError(translate("Enter a local file or folder path."));
    return;
  }
  const QFileInfo info(path);
  if (info.isDir())
    navigate(path);
  else if ((info.isFile() || mode_ == Mode::Save) && info.dir().exists()) {
    if (mode_ == Mode::Directory) {
      showError(translate("Choose a folder, not a file."));
      return;
    }
    navigate(info.absolutePath());
    filename_->setText(info.fileName());
    updatePreview(path);
    updateStatus();
  } else
    showError(translate("This file does not exist or cannot be read."));
}

void FilePickerDialog::activate(const QModelIndex &index) {
  const auto path = model_->filePath(proxy_->mapToSource(index));
  if (QFileInfo(path).isDir())
    navigate(path);
  else
    accept();
}

void FilePickerDialog::updateSelection() {
  const auto rows = details_->selectionModel()->selectedRows(0);
  if (rows.isEmpty()) {
    updateStatus();
    return;
  }
  const auto path = model_->filePath(proxy_->mapToSource(rows.first()));
  const QFileInfo info(path);
  filename_->setText(rows.size() == 1 ? info.fileName() : QString());
  error_->hide();
  updatePreview(path);
  updateStatus();
}

QStringList FilePickerDialog::candidatePaths() const {
  QStringList result;
  const auto rows = details_->selectionModel()->selectedRows(0);
  for (const auto &row : rows)
    result.append(model_->filePath(proxy_->mapToSource(row)));
  if (!result.isEmpty() && mode_ != Mode::Save)
    return result;
  if (!filename_->text().isEmpty() && mode_ != Mode::Directory) {
    const auto path = resolvePath(filename_->text());
    return path.isEmpty() ? QStringList{} : QStringList{path};
  }
  return mode_ == Mode::Directory ? QStringList{folder_} : QStringList{};
}

void FilePickerDialog::accept() {
  const auto paths = candidatePaths();
  if (paths.isEmpty()) {
    showError(translate("Select a file or enter its path."));
    return;
  }
  if (paths.size() == 1 && QFileInfo(paths.first()).isDir() &&
      mode_ != Mode::Directory) {
    navigate(paths.first());
    return;
  }
  for (const auto &path : paths) {
    const QFileInfo info(path);
    if (mode_ == Mode::Directory) {
      if (!info.isDir() || !info.isReadable()) {
        showError(translate("This folder does not exist or cannot be read."));
        return;
      }
    } else if (mode_ == Mode::Open) {
      if (!info.isFile() || !info.isReadable()) {
        showError(translate("This file does not exist or cannot be read."));
        return;
      }
    } else {
      if (!info.dir().exists() ||
          !QFileInfo(info.absolutePath()).isWritable() ||
          (info.exists() && (!info.isFile() || !info.isWritable()))) {
        showError(translate("This location cannot be written to."));
        return;
      }
      if (info.exists() &&
          QMessageBox::question(
              this, translate("Replace file?"),
              translate("A file with this name already exists. Replace it?"),
              QMessageBox::Yes | QMessageBox::No,
              QMessageBox::No) != QMessageBox::Yes)
        return;
    }
  }
  acceptedPaths_ = paths;
  QDialog::accept();
}

void FilePickerDialog::updateFilters() {
  QDir::Filters flags = QDir::AllDirs | QDir::NoDotAndDotDot;
  if (mode_ != Mode::Directory)
    flags |= QDir::Files;
  if (hidden_->isChecked())
    flags |= QDir::Hidden;
  model_->setFilter(flags);
  proxy_->apply(search_->text(), patterns_.value(filter_->currentIndex()));
  updateStatus();
}

void FilePickerDialog::updateStatus() {
  if (!accept_)
    return;
  const int count = proxy_->rowCount(details_->rootIndex());
  const auto selected = candidatePaths();
  status_->setText(
      translate("%1 items · %2 selected")
          .arg(count)
          .arg(details_->selectionModel()->selectedRows(0).size()));
  accept_->setEnabled(!selected.isEmpty());
}

void FilePickerDialog::showError(const QString &message) {
  error_->setText(message);
  error_->show();
}

void FilePickerDialog::updatePreview(const QString &path) {
  const QFileInfo info(path);
  previewImage_->setPixmap(
      fileIcon(info.isDir() ? FileIcon::Folder : FileIcon::File)
          .pixmap(64, 64));
  previewName_->setText(info.fileName().isEmpty() ? path : info.fileName());
  previewMeta_->setText(
      info.isDir()
          ? translate("Folder")
          : QLocale().formattedDataSize(info.size()) + "\n" +
                QLocale().toString(info.lastModified(), QLocale::ShortFormat));
  if (!info.isFile() || info.size() > 64 * 1024 * 1024)
    return;
  QImageReader reader(path);
  const auto dimensions = reader.size();
  if (!dimensions.isValid() ||
      qint64(dimensions.width()) * dimensions.height() > 32000000)
    return;
  reader.setAutoTransform(true);
  reader.setScaledSize(dimensions.scaled(180, 140, Qt::KeepAspectRatio));
  const auto image = reader.read();
  if (!image.isNull())
    previewImage_->setPixmap(QPixmap::fromImage(image));
}

QStringList FilePickerDialog::selectedPaths() const { return acceptedPaths_; }
int FilePickerDialog::selectedFilter() const { return filter_->currentIndex(); }

void FilePickerDialog::setFilters(
    const QList<QPair<QString, QStringList>> &filters, int current) {
  const QSignalBlocker blocker(filter_);
  filter_->clear();
  patterns_.clear();
  for (const auto &filter : filters) {
    filter_->addItem(filter.first);
    patterns_.append(filter.second);
  }
  filter_->setVisible(!filters.isEmpty() && mode_ != Mode::Directory);
  if (!filters.isEmpty())
    filter_->setCurrentIndex(qBound(0, current, int(filters.size()) - 1));
  updateFilters();
}

void FilePickerDialog::addChoice(const QString &id, const QString &label,
                                 const QList<QPair<QString, QString>> &values,
                                 const QString &selected) {
  auto *row = new QHBoxLayout;
  auto *caption = new QLabel(label, this);
  caption->setTextFormat(Qt::PlainText);
  row->addWidget(caption);
  auto *field = new QComboBox(this);
  for (const auto &value : values)
    field->addItem(value.second, value.first);
  if (values.isEmpty()) {
    field->addItem(translate("No"), "false");
    field->addItem(translate("Yes"), "true");
  }
  const int current = field->findData(selected);
  field->setCurrentIndex(current < 0 ? 0 : current);
  field->setAccessibleName(label);
  row->addWidget(field, 1);
  choices_->addLayout(row);
  choiceFields_.append({id, field});
}

QList<QPair<QString, QString>> FilePickerDialog::selectedChoices() const {
  QList<QPair<QString, QString>> result;
  for (const auto &choice : choiceFields_)
    result.append({choice.first, choice.second->currentData().toString()});
  return result;
}
} // namespace LunaDash
