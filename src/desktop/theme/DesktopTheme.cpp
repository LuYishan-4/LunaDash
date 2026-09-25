#include "desktop/theme/DesktopTheme.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "config/appearance/AppearancePalette.hpp"
#include <QMap>
#include <QRegularExpression>
#include <QColor>
#include <QTimer>
#include <QWidget>
namespace LunaDash {
QString desktopStyle() {
  QString style = R"(
QWidget { color: #edf3ff; font-family: 'Noto Sans', 'Noto Sans CJK TC', sans-serif; font-size: 13px; }
QFrame#desktopWindow { background: #0b1020; border: 1px solid #465777; border-radius: 12px; }
QFrame#titleBar { background: #162036; border-top-left-radius: 12px; border-top-right-radius: 12px; }
QFrame#panel { background: #141c31; border-bottom: 1px solid #3a4055; }
QFrame#dock, QFrame#launcher { background: #162036; border: 1px solid #4a5065; border-radius: 16px; }
QPushButton { background: #202b45; border: 1px solid transparent; border-radius: 12px; padding: 8px 14px; }
QPushButton:hover { background: #2c3b5c; border-color: #465777; }
QPushButton:pressed, QPushButton:checked { background: #2c3b5c; }
QPushButton:disabled { color: #aab7d1; }
QPushButton#accent { background: #9ccbfb; color: #10182a; font-weight: bold; }
QPushButton#panelButton { background: transparent; font-weight: bold; }
QPushButton#windowControl, QPushButton#closeWindow { padding: 0; background: transparent; }
QPushButton#closeWindow:hover { background: #be626f; }
QPushButton#dockButton { font-size: 22px; padding: 5px; background: #303b53; }
QPushButton#dockButton:checked { border-bottom: 3px solid #bfcef8; }
QLabel#muted { color: #aab7d1; font-size: 11px; }
QLabel#eyebrow { color: #9ccbfb; font-size: 11px; letter-spacing: 2px; }
QLabel#heroTitle { font-size: 32px; font-weight: bold; }
QLabel#heading { font-size: 24px; font-weight: bold; }
QLabel#description { color: #aab7d1; font-size: 14px; }
QWidget#welcomePage { background: #0a0f1d; }
QFrame#welcomeHero { background: #0f1728; border: 1px solid #52698d; border-radius: 24px; }
QFrame#welcomeCard { background: #10192c; border: 1px solid #2f4263; border-radius: 18px; min-height: 122px; }
QFrame#welcomeMark { background: #121f36; border: 1px solid #52698d; border-radius: 28px; }
QLabel#welcomeEyebrow { color: #9ccbfb; font-size: 10px; font-weight: 600; letter-spacing: 2px; }
QLabel#welcomeHeroTitle { color: #edf3ff; font-size: 30px; font-weight: 650; }
QLabel#welcomeHeroDescription { color: #b7c5df; font-size: 13px; }
QLabel#welcomeValue { color: #edf3ff; font-size: 17px; font-weight: 600; }
QLabel#welcomeDetail { color: #aab7d1; font-size: 11px; }
QLabel#welcomeMeta { color: #9fb1d0; font-size: 9px; letter-spacing: 1px; }
QLabel#welcomeGlyph { color: #9ccbfb; font-size: 72px; }
QPushButton#welcomeAction { background: #182541; border: 1px solid #334766; border-radius: 12px; }
QPushButton#welcomeAction:hover { background: #223454; border-color: #607aa2; }

QLineEdit, QPlainTextEdit, QTreeView, QListView { background: #141c31; border: 1px solid #465777; border-radius: 11px; padding: 9px; selection-background-color: #2c3b5c; }
QLineEdit#portalLocation { background: #10192c; border-radius: 12px; padding: 9px 12px; }
QLineEdit#portalLocation[invalidPath="true"] { border-color: #f2b8c6; }
QDialog#screenShareChooser { background: #0b1020; }
QDialog#portalFileShell { background: transparent; }
QFrame#portalFileFrame { background: #0b1020; border: 1px solid #52698d; border-radius: 24px; }
QFrame#portalFileHeader { background: #10192c; border: 1px solid #2f4263; border-radius: 16px; }
QLabel#portalFileHeading { color: #edf3ff; font-size: 21px; font-weight: 600; }
QLabel#portalFileCaption { color: #aab7d1; font-size: 11px; }
QToolButton#portalFileClose { background: transparent; border: 1px solid transparent; border-radius: 10px; color: #dbe6fa; font-size: 18px; }
QToolButton#portalFileClose:hover { background: #3b2532; border-color: #8e5364; }
QDialog#portalFileShell QListWidget#filePlaces { background: #10192c; border: 1px solid #2f4263; border-radius: 16px; padding: 8px; }
QDialog#portalFileShell QPushButton#accent:disabled { background: #263650; color: #8a9bb8; }
QDialog#portalFileShell QLineEdit { min-height: 22px; }
QDialog#portalFileShell QSplitter::handle { background: transparent; width: 14px; }

QListWidget#screenShareSources { background: #10192c; border: 1px solid #465777; border-radius: 18px; padding: 12px; outline: none; }
QListWidget#screenShareSources::item { background: #162036; border: 1px solid #334766; border-radius: 16px; padding: 10px; margin: 3px; }
QListWidget#screenShareSources::item:hover { background: #202b45; border-color: #607aa2; }
QListWidget#screenShareSources::item:selected { background: #243755; border-color: #9ccbfb; }
QTreeView { alternate-background-color: #162036; }
QTreeView::item { min-height: 30px; }
QHeaderView::section { background: #1a2440; color: #aab7d1; border: none; padding: 7px; }
QListWidget::item { padding: 10px; border-radius: 5px; }
QListWidget::item:selected { background: #2c3b5c; }
QScrollBar:horizontal { background: #162036; height: 9px; }
QScrollBar::handle:horizontal { background: #465777; border-radius: 4px; min-width: 22px; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QScrollBar:vertical { background: transparent; width: 9px; }
QScrollBar::handle:vertical { background: #465777; border-radius: 4px; min-height: 22px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QProgressBar { background: #121e20; border: none; border-radius: 6px; min-height: 34px; text-align: center; }
QProgressBar::chunk { background: #467579; border-radius: 6px; }
QToolTip { background: #1a2440; color: #eef2ff; border: 1px solid #566783; }
QMessageBox, QFileDialog, QDialog, QMenu { background: #0b1020; }
QMenu { border: 1px solid #465777; border-radius: 10px; padding: 7px; }
QMenu::item { padding: 8px 26px; border-radius: 7px; }
QMenu::item:selected { background: #2c3b5c; }
QMenu::item:disabled { color: #aab7d1; }
QMenu::separator { height: 1px; background: #465777; margin: 6px 10px; }
QCheckBox { spacing: 8px; padding: 4px; }
QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid #465777; border-radius: 4px; background: #141c31; }
QCheckBox::indicator:checked { background: #9ccbfb; border-color: #9ccbfb; }
)";
  const auto preferences = desktopPreferences();
  const auto colors = appearancePalette(preferences);
  style += R"(
QLabel#fileStatus { color: #aab7d1; font-size: 11px; }
QPushButton[fileTool="true"] { border: none; padding: 6px; border-radius: 10px; background: transparent; }
QPushButton[fileTool="true"]:hover { background: #2c3b5c; }
QPushButton[fileTool="true"]:checked { background: #2c3b5c; }
QListWidget#filePlaces { background: transparent; border: none; padding: 0; outline: none; }
QListWidget#filePlaces::item { padding: 10px 12px; border-radius: 10px; }
QListWidget#filePlaces::item:hover { background: #202b45; }
QListWidget#filePlaces::item:selected { background: #2c3b5c; }
QTreeView#fileView, QListView#fileIcons { background: #141c31; border: 1px solid #465777; border-radius: 15px; padding: 16px; outline: none; }
QTreeView#fileView::item { min-height: 42px; border: none; }
QTreeView#fileView::item:hover, QListView#fileIcons::item:hover { background: #202b45; }
QTreeView#fileView::item:selected, QListView#fileIcons::item:selected { background: #2c3b5c; }
QHeaderView::section { background: #141c31; color: #aab7d1; font-size: 11px; padding: 12px 8px; }
QFrame#filePreviewPanel { background: #162036; border: 1px solid #465777; border-radius: 16px; }
QLabel#filePreviewEyebrow { color: #9ccbfb; font-size: 10px; font-weight: 600; letter-spacing: 2px; }
QLabel#filePreviewImage { background: #141c31; border: 1px solid #2b3b4c; border-radius: 14px; padding: 12px; color: #aab7d1; }
QLabel#filePreviewTitle { font-size: 16px; font-weight: 600; }
QLabel#filePreviewMeta { color: #aab7d1; font-size: 11px; }
QLabel#filePreviewHint { color: #aab7d1; font-size: 10px; }
)";
  style +=
      QString(
          "QWidget#applicationWindow, QWidget#applicationContent { background: "
          "%1; border-radius: 14px; } "
          "QListView#fileIcons::item { border-radius: 10px; padding: 8px; } "
          "QListView#fileIcons::item:selected { background: %2; } "
          "QSplitter::handle { background: transparent; width: 12px; }")
          .arg(QStringLiteral("#0b1020"), QStringLiteral("#202b45"));
  style += R"(
QWidget#pluginPage, QWidget#pluginStore, QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; }
QFrame#pluginCard { background: #141c31; border: 1px solid #465777; border-radius: 18px; }
QLabel#pluginName { font-size: 16px; font-weight: 600; }
QLabel#pluginIcon { background: #202b45; border-radius: 12px; color: #9ccbfb; font-weight: 600; }
QLabel#danger { color: #f2b8c6; }
QTabWidget::pane { border: none; background: transparent; }
QTabBar::tab { background: #202b45; color: #aab7d1; padding: 10px 24px; margin: 0 6px 12px 0; border: 1px solid transparent; border-radius: 12px; }
QTabBar::tab:selected { background: #2c3b5c; color: #edf3ff; border-color: #465777; }
QTabBar::tab:hover { background: #2c3b5c; color: #edf3ff; }
QToolButton { background: #202b45; border: 1px solid transparent; border-radius: 10px; padding: 7px; }
QToolButton:hover, QToolButton:checked { background: #2c3b5c; border-color: #465777; }
QToolButton:disabled { color: #aab7d1; }
QComboBox, QSpinBox { background: #202b45; border: 1px solid #465777; border-radius: 10px; padding: 8px 12px; min-height: 20px; }
QComboBox::drop-down { border: none; width: 24px; }
QComboBox QAbstractItemView { background: #1a2440; color: #edf3ff; border: 1px solid #465777; selection-background-color: #2c3b5c; }
QLineEdit:focus, QComboBox:focus, QPushButton:focus, QToolButton:focus { border: 1px solid #e8f0ff; }
QPushButton:default { background: #9ccbfb; color: #10182a; font-weight: 600; }
QPushButton:default:hover { border-color: #e8f0ff; }
QCheckBox:focus { color: #9ccbfb; }
QCheckBox::indicator { width: 20px; height: 20px; border-radius: 7px; }
QCheckBox::indicator:checked { image: url(:/qt-project.org/styles/commonstyle/images/standardbutton-apply-16.png); }
QFileDialog { background: #0b1020; }
QFileDialog QListView, QFileDialog QTreeView { background: #141c31; border: 1px solid #465777; border-radius: 16px; padding: 10px; }
QFileDialog QTreeView::item { min-height: 34px; }
QHeaderView, QTableCornerButton::section { background: #141c31; border: none; }
QFileDialog QHeaderView::section { background: #141c31; color: #aab7d1; padding: 10px 8px; }
QAbstractItemView::item:hover { background: #202b45; }
QAbstractItemView::item:selected { background: #2c3b5c; color: #edf3ff; }
)";
  // Match the shell Theme palette, and substitute preferences after all rules
  // so later file-manager and dialog rules cannot restore the default accent.
  const QMap<QString, QString> roles{
      {"#9ccbfb", "accent"}, {"#0b1020", "background"}, {"#0a0f1d", "background"},
      {"#0f1728", "background"}, {"#10192c", "surface"}, {"#141c31", "surface"},
      {"#162036", "surfaceElevated"}, {"#202b45", "surfaceElevated"},
      {"#2c3b5c", "surfaceHover"}, {"#1a2440", "surfaceElevated"},
      {"#edf3ff", "text"}, {"#eef2ff", "text"}, {"#aab7d1", "muted"},
      {"#b7c5df", "muted"}, {"#9fb1d0", "muted"}, {"#10182a", "accentInk"},
      {"#465777", "border"}, {"#52698d", "border"}, {"#2f4263", "hairline"},
      {"#334766", "hairline"}, {"#607aa2", "border"}, {"#e8f0ff", "accent"},
      {"#f2b8c6", "danger"}};
  const QRegularExpression colorPattern("#[0-9a-fA-F]{6}");
  auto matches = colorPattern.globalMatch(style);
  QString themed;
  qsizetype offset = 0;
  while (matches.hasNext()) {
    const auto match = matches.next();
    themed += style.mid(offset, match.capturedStart() - offset);
    const auto role = roles.value(match.captured().toLower());
    themed += role.isEmpty() ? match.captured() : colors.value(role).toString();
    offset = match.capturedEnd();
  }
  style = themed + style.mid(offset);
  // The LunaDash portal retains its owned dark header in both theme modes.
  style += " QFrame#portalFileHeader { background: #191c22; border-color: #353b46; }"
           " QLabel#portalFileHeading { color: #e2e5ed; }"
           " QLabel#portalFileCaption { color: #bdc5d3; }";
  auto font = preferences.value("fontFamily").toString();
  font.replace("\\", "\\\\");
  font.replace("'", "\\'");
  style.replace("'Noto Sans', 'Noto Sans CJK TC', sans-serif",
                "'" + font + "'");
  return style;
}
void watchDesktopTheme(QWidget *window) {
  const auto applyPalette = [window] {
    QPalette palette = window->palette();
    const auto colors = appearancePalette(desktopPreferences());
    const auto color = [&colors](const QString &key) { return QColor(colors.value(key).toString()); };
    palette.setColor(QPalette::Window, color("background"));
    palette.setColor(QPalette::WindowText, color("text"));
    palette.setColor(QPalette::Base, color("surface"));
    palette.setColor(QPalette::AlternateBase, color("surfaceElevated"));
    palette.setColor(QPalette::Text, color("text"));
    palette.setColor(QPalette::Button, color("surfaceElevated"));
    palette.setColor(QPalette::ButtonText, color("text"));
    palette.setColor(QPalette::PlaceholderText, color("muted"));
    palette.setColor(QPalette::Highlight,
                     color("accent"));
    palette.setColor(QPalette::HighlightedText, color("accentInk"));
    palette.setColor(QPalette::Disabled, QPalette::Text, color("muted"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText,
                     color("muted"));
    window->setPalette(palette);
    window->setProperty("ludashAccent", palette.color(QPalette::Highlight));
  };
  applyPalette();
  const auto styled = [window] {
    auto next = desktopStyle();
    if (window->property("ludashFrosted").toBool())
      next += QStringLiteral(
          " QWidget#applicationWindow { background: transparent; }");
    return next;
  };
  window->setStyleSheet(styled());
  auto *timer = new QTimer(window);
  timer->setInterval(700);
  QObject::connect(timer, &QTimer::timeout, window,
                   [window, styled, applyPalette] {
                     const auto next = styled();
                     if (window->styleSheet() != next) {
                       applyPalette();
                       window->setStyleSheet(next);
                     }
                   });
  timer->start();
}
} // namespace LunaDash
