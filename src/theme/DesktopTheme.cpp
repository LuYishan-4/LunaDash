#include <LuDash/theme/DesktopTheme.h>
#include <LuDash/configuration/DesktopPreferences.h>
#include <QWidget>
#include <QTimer>
#include <QColor>
namespace LuDash {
QString desktopStyle() { QString style = R"(
QWidget { color: #d6dfdc; font-family: 'Noto Sans', 'Noto Sans CJK TC', sans-serif; font-size: 13px; }
QFrame#desktopWindow { background: #151d26; border: 1px solid #71879e; border-radius: 12px; }
QFrame#titleBar { background: #202e30; border-top-left-radius: 12px; border-top-right-radius: 12px; }
QFrame#panel { background: #151b2a; border-bottom: 1px solid #3a4055; }
QFrame#dock, QFrame#launcher { background: #1c2334; border: 1px solid #4a5065; border-radius: 16px; }
QPushButton { background: #253443; border: 1px solid transparent; border-radius: 7px; padding: 8px 14px; }
QPushButton:hover { background: #344a60; border-color: #71879e; }
QPushButton:pressed, QPushButton:checked { background: #436185; }
QPushButton:disabled { color: #818a9e; }
QPushButton#accent { background: #9ccbfb; color: #172628; font-weight: bold; }
QPushButton#panelButton { background: transparent; font-weight: bold; }
QPushButton#windowControl, QPushButton#closeWindow { padding: 0; background: transparent; }
QPushButton#closeWindow:hover { background: #be626f; }
QPushButton#dockButton { font-size: 22px; padding: 5px; background: #303b53; }
QPushButton#dockButton:checked { border-bottom: 3px solid #bfcef8; }
QLabel#muted { color: #96adad; font-size: 11px; }
QLabel#eyebrow { color: #9ccbfb; font-size: 11px; letter-spacing: 2px; }
QLabel#heroTitle { font-size: 32px; font-weight: bold; }
QLabel#heading { font-size: 24px; font-weight: bold; }
QLabel#description { color: #adbfbd; font-size: 14px; }
QLineEdit, QPlainTextEdit, QTreeView, QListView { background: #141c26; border: 1px solid #405f62; border-radius: 7px; padding: 9px; selection-background-color: #436185; }
QTreeView { alternate-background-color: #1b2a2c; }
QTreeView::item { min-height: 30px; }
QHeaderView::section { background: #273c3f; color: #adbfbd; border: none; padding: 7px; }
QListWidget::item { padding: 10px; border-radius: 5px; }
QListWidget::item:selected { background: #3d5e61; }
QScrollBar:horizontal { background: #1b2a2c; height: 9px; }
QScrollBar::handle:horizontal { background: #496d70; border-radius: 4px; min-width: 22px; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QScrollBar:vertical { background: transparent; width: 9px; }
QScrollBar::handle:vertical { background: #496d70; border-radius: 4px; min-height: 22px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QProgressBar { background: #121e20; border: none; border-radius: 6px; min-height: 34px; text-align: center; }
QProgressBar::chunk { background: #467579; border-radius: 6px; }
QToolTip { background: #273c3f; color: #eef2ff; border: 1px solid #566783; }
QMessageBox, QFileDialog { background: #151d26; }
)";
    const auto preferences = desktopPreferences();
    const QColor accent(preferences.value("accent").toString());
    const auto tint = [&accent](int base, double amount) { return QColor(static_cast<int>(base * (1 - amount) + accent.red() * amount), static_cast<int>(base * (1 - amount) + accent.green() * amount), static_cast<int>(base * (1 - amount) + accent.blue() * amount)).name(); };
    style.replace("#9ccbfb", accent.name());
    for (const auto& color : {"#436185", "#3d5e61", "#467579"}) style.replace(color, tint(24, .38));
    for (const auto& color : {"#253443", "#273c3f", "#202e30", "#1b2a2c"}) style.replace(color, tint(20, .10));
    for (const auto& color : {"#71879e", "#405f62", "#496d70"}) style.replace(color, tint(40, .32));
    style += QString("QWidget#applicationWindow, QWidget#applicationContent { background: %1; border-radius: 14px; } QListView#fileIcons::item { border-radius: 10px; padding: 8px; } QListView#fileIcons::item:selected { background: %2; } QSplitter::handle { background: transparent; width: 12px; }").arg(tint(18, .05), tint(24, .38));
    style.replace("'Noto Sans', 'Noto Sans CJK TC', sans-serif", preferences.value("fontFamily").toString());
    return style;
}
void watchDesktopTheme(QWidget* window) {
    window->setStyleSheet(desktopStyle());
    auto* timer = new QTimer(window); timer->setInterval(700);
    QObject::connect(timer, &QTimer::timeout, window, [window] { const auto next = desktopStyle(); if (window->styleSheet() != next) window->setStyleSheet(next); });
    timer->start();
}
}
