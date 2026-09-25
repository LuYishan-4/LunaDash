#include "service/portal/FileChooserOptions.hpp"
#include "service/portal/FileChooserPortal.hpp"
#include "service/portal/FilePickerDialog.hpp"
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeView>
#include <QUrl>
#include <QtTest>

namespace LunaDash {
class PortalPickerTests final : public QObject {
  Q_OBJECT
  QTemporaryDir directory_;
  QTemporaryDir config_;

  void pastePath(FilePickerDialog &picker, const QString &path) {
    auto *field = picker.findChild<QLineEdit *>("portalFilename");
    field->setFocus();
    field->selectAll();
    QApplication::clipboard()->setText(path);
    QTest::keyClick(field, Qt::Key_V, Qt::ControlModifier);
  }

  void chooseOnOpen(const QString &path) {
    QTimer::singleShot(0, qApp, [this, path] {
      auto *picker =
          dynamic_cast<FilePickerDialog *>(QApplication::activeModalWidget());
      if (!picker)
        qFatal("No portal picker was shown");
      QTimer::singleShot(3000, picker, &QDialog::reject);
      pastePath(*picker, path);
      picker->findChild<QPushButton *>("accent")->click();
    });
  }

private slots:
  void initTestCase() {
    QVERIFY(directory_.isValid());
    QVERIFY(config_.isValid());
    qputenv("XDG_CONFIG_HOME", config_.path().toUtf8());
    for (const auto &name : {"first track.txt", "second.txt"}) {
      QFile file(directory_.filePath(name));
      QVERIFY(file.open(QIODevice::WriteOnly));
      file.write("picker fixture");
    }
  }

  void selectionSurvivesCloseAndNextRequest() {
    FileChooserPortal portal;
    const auto path = directory_.filePath("first track.txt");
    const auto uri = QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded);
    for (int repeat = 0; repeat < 3; ++repeat) {
      chooseOnOpen(uri);
      QVariantMap results;
      QCOMPARE(portal.OpenFile(QDBusObjectPath("/test"), {}, {}, "Open file",
                               {{"current_folder", directory_.path().toUtf8()}},
                               results),
               0u);
      QCOMPARE(results.value("uris").toStringList(), QStringList{uri});
    }
  }

  void multipleSelectionAndFilter() {
    FilePickerDialog picker(
        FilePickerDialog::Mode::Open, "Choose files",
        {{"current_folder", directory_.path().toUtf8()}, {"multiple", true}});
    QList<PortalFileFilter> filters{{"Text files", {{0, "*.txt"}}}};
    const QVariantMap options{{"filters", QVariant::fromValue(filters)}};
    configureFilePicker(picker, options);
    picker.show();
    auto *view = picker.findChild<QTreeView *>("fileView");
    QTRY_COMPARE_WITH_TIMEOUT(view->model()->rowCount(view->rootIndex()), 2,
                              3000);
    for (int row = 0; row < 2; ++row)
      view->selectionModel()->select(
          view->model()->index(row, 0, view->rootIndex()),
          QItemSelectionModel::Select | QItemSelectionModel::Rows);
    picker.grab().save("portal-picker.png");
    picker.findChild<QPushButton *>("accent")->click();
    QCOMPARE(picker.result(), int(QDialog::Accepted));
    QCOMPARE(picker.selectedPaths().size(), 2);
    const auto result = filePickerResults(picker, options);
    QCOMPARE(
        qvariant_cast<PortalFileFilter>(result.value("current_filter")).label,
        QString("Text files"));
  }

  void invalidPathDoesNotAccept() {
    FilePickerDialog picker(FilePickerDialog::Mode::Open, "Open file", {});
    picker.show();
    pastePath(picker, directory_.filePath("missing.txt"));
    picker.findChild<QPushButton *>("accent")->click();
    QVERIFY(picker.isVisible());
    QVERIFY(picker.selectedPaths().isEmpty());
    picker.reject();
  }

  void locationEnterOnlySelects() {
    FilePickerDialog picker(FilePickerDialog::Mode::Open, "Open file", {});
    picker.show();
    auto *location = picker.findChild<QLineEdit *>("portalLocation");
    location->setFocus();
    location->setText(
        QUrl::fromLocalFile(directory_.filePath("first track.txt")).toString());
    QTest::keyClick(location, Qt::Key_Return);
    QVERIFY(picker.isVisible());
    QVERIFY(picker.selectedPaths().isEmpty());
    picker.findChild<QPushButton *>("accent")->click();
    QCOMPARE(picker.selectedPaths(),
             QStringList{directory_.filePath("first track.txt")});
  }

  void saveNewFileAndRejectTraversal() {
    FileChooserPortal portal;
    const auto path = directory_.filePath("new file.txt");
    chooseOnOpen(path);
    QVariantMap result;
    QCOMPARE(portal.SaveFile(QDBusObjectPath("/test"), {}, {}, "Save file", {},
                             result),
             0u);
    QCOMPARE(
        result.value("uris").toStringList(),
        QStringList{QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded)});
    QVERIFY(!QFile::exists(path));
    const QVariantMap options{
        {"files", QVariant::fromValue(QList<QByteArray>{"../escape.txt"})}};
    QCOMPARE(
        portal.SaveFiles(QDBusObjectPath("/test"), {}, {}, {}, options, result),
        2u);
    QVERIFY(result.isEmpty());
  }
};
} // namespace LunaDash
QTEST_MAIN(LunaDash::PortalPickerTests)
#include "PortalPickerTests.moc"
