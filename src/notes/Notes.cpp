#include <LuDash/localization/Localization.h>
#include <LuDash/notes/Notes.h>
#include <QtWidgets>
#include <memory>

namespace LuDash {
QWidget* createNotes(std::function<bool()>& canClose) {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    auto* bar = new QHBoxLayout;
    auto* open = new QPushButton(LuDash::translate("Open"));
    auto* save = new QPushButton(LuDash::translate("Save"));
    save->setObjectName("accent");
    auto* name = new QLabel(LuDash::translate("Untitled.txt"));
    bar->addWidget(open); bar->addWidget(save); bar->addWidget(name, 1);
    layout->addLayout(bar);
    auto* editor = new QPlainTextEdit;
    editor->setObjectName("notesEditor");
    editor->setPlaceholderText(LuDash::translate("Capture an idea here.\n\nPress Ctrl+S to save."));
    layout->addWidget(editor, 1);
    auto* status = new QLabel(LuDash::translate("UTF-8  ·  0 characters"));
    status->setObjectName("muted");
    layout->addWidget(status);
    auto path = std::make_shared<QString>();
    auto saveFile = [=]() -> bool {
        QString selected = *path;
        if (selected.isEmpty()) selected = QFileDialog::getSaveFileName(page, LuDash::translate("Save note"), QDir::homePath() + LuDash::translate("/Untitled.txt"));
        if (selected.isEmpty()) return false;
        QSaveFile file(selected);
        const auto bytes = editor->toPlainText().toUtf8();
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
            QMessageBox::warning(page, LuDash::translate("Save failed"), file.errorString()); return false;
        }
        *path = selected; name->setText(QFileInfo(selected).fileName());
        editor->document()->setModified(false);
        return true;
    };
    canClose = [=]() {
        if (!editor->document()->isModified()) return true;
        auto answer = QMessageBox::question(page, LuDash::translate("Unsaved changes"), LuDash::translate("Save changes to this note?"), QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        return answer == QMessageBox::Discard || (answer == QMessageBox::Save && saveFile());
    };
    QObject::connect(save, &QPushButton::clicked, page, [=] { saveFile(); });
    auto* shortcut = new QShortcut(QKeySequence::Save, page);
    shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(shortcut, &QShortcut::activated, page, [=] { saveFile(); });
    QObject::connect(open, &QPushButton::clicked, page, [=, guard = canClose] {
        if (!guard()) return;
        const auto selected = QFileDialog::getOpenFileName(page, LuDash::translate("Open text file"), QDir::homePath(), LuDash::translate("Text files (*.txt *.md *.cpp *.h);;All files (*)"));
        if (selected.isEmpty()) return;
        QFile file(selected);
        if (!file.open(QIODevice::ReadOnly)) { QMessageBox::warning(page, LuDash::translate("Unable to open"), file.errorString()); return; }
        if (file.size() > 8 * 1024 * 1024) { QMessageBox::warning(page, LuDash::translate("File too large"), LuDash::translate("Open a text file smaller than 8 MiB.")); return; }
        editor->setPlainText(QString::fromUtf8(file.readAll()));
        *path = selected; name->setText(QFileInfo(selected).fileName());
        editor->document()->setModified(false);
    });
    QObject::connect(editor, &QPlainTextEdit::textChanged, page, [=] { status->setText(QString(LuDash::translate("UTF-8  ·  %1 characters")).arg(editor->toPlainText().size())); });
    return page;
}
}
