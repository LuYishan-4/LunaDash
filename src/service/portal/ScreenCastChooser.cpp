#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTextStream>
#include <QVBoxLayout>
#include <QVariant>

#include <cstdio>

namespace {

QString displayLabel(const QString &source) {
  if (source.startsWith(QStringLiteral("Monitor: ")))
    return QObject::tr("Screen — %1").arg(source.mid(9));
  if (source.startsWith(QStringLiteral("Window: ")))
    return QObject::tr("Window — %1").arg(source.mid(8));
  return source;
}

QStringList readSources() {
  QStringList sources;
  QTextStream input(stdin, QIODevice::ReadOnly);
  while (!input.atEnd()) {
    const QString source = input.readLine().trimmed();
    if (!source.isEmpty())
      sources.append(source);
  }
  return sources;
}

int printSource(const QString &source) {
  QTextStream output(stdout, QIODevice::WriteOnly);
  output << source << Qt::endl;
  output.flush();
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  const QStringList sources = readSources();
  if (sources.isEmpty())
    return 1;

  // CI exercises the same stdin/stdout chooser contract without requiring a
  // graphical session. xdg-desktop-portal-wlr never sets this variable.
  if (qEnvironmentVariableIntValue(
          "LUNADASH_SCREENCAST_CHOOSER_AUTOPICK") == 1)
    return printSource(sources.constFirst());

  QApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("LunaDash Screen Share"));
  app.setOrganizationName(QStringLiteral("LunaDash"));

  QDialog dialog;
  dialog.setWindowTitle(QObject::tr("Share your screen"));
  dialog.setModal(true);
  dialog.resize(640, 420);

  auto *layout = new QVBoxLayout(&dialog);

  auto *title = new QLabel(QObject::tr("Choose what to share"), &dialog);
  QFont titleFont = title->font();
  titleFont.setBold(true);
  titleFont.setPointSizeF(titleFont.pointSizeF() + 2.0);
  title->setFont(titleFont);
  layout->addWidget(title);

  auto *description = new QLabel(
      QObject::tr("Select a screen or an application window. "
                  "Screen sharing never starts the screenshot region selector."),
      &dialog);
  description->setWordWrap(true);
  layout->addWidget(description);

  auto *list = new QListWidget(&dialog);
  list->setSelectionMode(QAbstractItemView::SingleSelection);
  for (const QString &source : sources) {
    auto *item = new QListWidgetItem(displayLabel(source), list);
    item->setData(Qt::UserRole, source);
  }
  list->setCurrentRow(0);
  layout->addWidget(list, 1);

  auto *buttons =
      new QDialogButtonBox(QDialogButtonBox::Cancel, &dialog);
  auto *share =
      buttons->addButton(QObject::tr("Share"), QDialogButtonBox::AcceptRole);
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
