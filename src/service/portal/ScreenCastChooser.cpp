#include "service/portal/ScreenCastChooser.hpp"
#include "desktop/theme/DesktopTheme.hpp"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QIcon>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QPixmap>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTextStream>
#include <QVBoxLayout>
#include <QVariant>

namespace LunaDash {
namespace {

QString displayLabel(const QString &source) {
  if (source.startsWith(QStringLiteral("Monitor: ")))
    return QObject::tr("Screen — %1").arg(source.mid(9));
  if (source.startsWith(QStringLiteral("Window: ")))
    return QObject::tr("Window — %1").arg(source.mid(8));
  return source;
}

QString sourceWindowTitle(const QString &source) {
  return source.startsWith(QStringLiteral("Window: ")) ? source.mid(8).trimmed()
                                                       : QString();
}

QStringList readSources() {
  QStringList sources;
  QTextStream input(stdin, QIODevice::ReadOnly);
  input.setEncoding(QStringConverter::Utf8);
  while (!input.atEnd()) {
    // xdpw matches this opaque label byte-for-byte. Whitespace is part of
    // the monitor/window identity, not formatting to normalize.
    const QString source = input.readLine();
    if (!source.isEmpty())
      sources.append(source);
  }
  return sources;
}

int printSource(const QString &source) {
  QTextStream output(stdout, QIODevice::WriteOnly);
  output.setEncoding(QStringConverter::Utf8);
  output << source << Qt::endl;
  output.flush();
  return 0;
}

QJsonObject compositorState() {
  const QString control = QStandardPaths::findExecutable("lunadashctl");
  if (control.isEmpty())
    return {};
  QProcess process;
  process.start(control, {"status"});
  if (!process.waitForStarted(500) || !process.waitForFinished(1200) ||
      process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    return {};
  const auto document = QJsonDocument::fromJson(process.readAllStandardOutput());
  return document.isObject() ? document.object() : QJsonObject{};
}

QImage desktopFrame() {
  const QString grim = QStandardPaths::findExecutable("grim");
  if (grim.isEmpty())
    return {};
  QProcess process;
  process.start(grim, {"-"});
  if (!process.waitForStarted(500) || !process.waitForFinished(1600) ||
      process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    return {};
  return QImage::fromData(process.readAllStandardOutput(), "PNG");
}

QPixmap previewForSource(const QString &source, const QJsonObject &state,
                         const QImage &frame) {
  if (frame.isNull())
    return {};

  QRect crop(0, 0, frame.width(), frame.height());
  const QString title = sourceWindowTitle(source);
  if (!title.isEmpty()) {
    bool found = false;
    for (const auto &value : state.value("clients").toArray()) {
      const auto client = value.toObject();
      const QString candidate = client.value("title").toString();
      if (candidate == title ||
          (!candidate.isEmpty() &&
           (title.contains(candidate) || candidate.contains(title)))) {
        const QRect geometry(client.value("x").toInt(), client.value("y").toInt(),
                             client.value("width").toInt(),
                             client.value("height").toInt());
        const QRect bounded = geometry.intersected(frame.rect());
        if (bounded.isValid() && bounded.width() > 8 && bounded.height() > 8) {
          crop = bounded;
          found = true;
          break;
        }
      }
    }
    if (!found)
      return {};
  }

  return QPixmap::fromImage(frame.copy(crop)).scaled(
      300, 170, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

class SourceList final : public QListWidget {
public:
  using QListWidget::QListWidget;
  QSize sizeHint() const override { return {760, 390}; }
};

} // namespace

int runScreenCastChooser(int argc, char **argv) {
  const QStringList sources = readSources();
  if (sources.isEmpty())
    return 1;

  if (qEnvironmentVariableIntValue(
          "LUNADASH_SCREENCAST_CHOOSER_AUTOPICK") == 1)
    return printSource(sources.constFirst());

  (void)argc;
  int qtArgc = 1;
  char *qtArgv[] = {argv[0], nullptr};
  QApplication app(qtArgc, qtArgv);
  // Share the compositor's QSettings namespace so the chooser follows the
  // current LunaDash light/dark mode, palette and animation preference.
  app.setApplicationName(QStringLiteral("LunaDash"));
  app.setApplicationDisplayName(QStringLiteral("LunaDash Screen Share"));
  app.setOrganizationName(QStringLiteral("LunaDash"));
  QApplication::setStyle("Fusion");

  const QJsonObject state = compositorState();
  const QImage frame = desktopFrame();

  QDialog dialog;
  dialog.setObjectName(QStringLiteral("screenShareChooser"));
  dialog.setWindowTitle(QObject::tr("Share your screen"));
  dialog.setModal(true);
  dialog.resize(860, 650);
  dialog.setMinimumSize(720, 520);
  LunaDash::watchDesktopTheme(&dialog);

  auto *layout = new QVBoxLayout(&dialog);
  layout->setContentsMargins(22, 22, 22, 22);
  layout->setSpacing(14);

  auto *title = new QLabel(QObject::tr("Choose what to share"), &dialog);
  title->setObjectName(QStringLiteral("heading"));
  layout->addWidget(title);

  auto *description = new QLabel(
      QObject::tr("Select a screen or application window. Visible LunaDash "
                  "windows use a live frame preview captured when this chooser "
                  "opens; unavailable or minimized windows keep a labelled card."),
      &dialog);
  description->setObjectName(QStringLiteral("description"));
  description->setWordWrap(true);
  layout->addWidget(description);

  auto *list = new SourceList(&dialog);
  list->setObjectName(QStringLiteral("screenShareSources"));
  list->setSelectionMode(QAbstractItemView::SingleSelection);
  list->setViewMode(QListView::IconMode);
  list->setResizeMode(QListView::Adjust);
  list->setMovement(QListView::Static);
  list->setWrapping(true);
  list->setSpacing(12);
  list->setIconSize(QSize(300, 170));
  list->setGridSize(QSize(350, 230));
  list->setWordWrap(true);

  for (const QString &source : sources) {
    auto *item = new QListWidgetItem(displayLabel(source), list);
    item->setData(Qt::UserRole, source);
    const QPixmap preview = previewForSource(source, state, frame);
    if (!preview.isNull())
      item->setIcon(QIcon(preview));
    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);
    item->setToolTip(displayLabel(source));
  }
  list->setCurrentRow(0);
  layout->addWidget(list, 1);

  auto *hint = new QLabel(
      frame.isNull()
          ? QObject::tr("Preview capture is unavailable; source selection still works.")
          : QObject::tr("Previews are local and are not shared until you confirm."),
      &dialog);
  hint->setObjectName(QStringLiteral("muted"));
  layout->addWidget(hint);

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dialog);
  auto *share =
      buttons->addButton(QObject::tr("Share"), QDialogButtonBox::AcceptRole);
  share->setObjectName(QStringLiteral("accent"));
  share->setDefault(true);
  share->setEnabled(list->currentItem() != nullptr);
  layout->addWidget(buttons);

  QObject::connect(list, &QListWidget::currentItemChanged, &dialog,
                   [share](QListWidgetItem *current) {
                     share->setEnabled(current != nullptr);
                   });
  QObject::connect(list, &QListWidget::itemDoubleClicked, &dialog,
                   [&dialog](QListWidgetItem *) { dialog.accept(); });
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog,
                   &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog,
                   &QDialog::reject);

  if (dialog.exec() != QDialog::Accepted)
    return 0;

  const auto *selected = list->currentItem();
  if (!selected)
    return 1;
  return printSource(selected->data(Qt::UserRole).toString());
}

} // namespace LunaDash
