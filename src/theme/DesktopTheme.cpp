#include <LuDash/theme/DesktopTheme.h>
namespace LuDash {
QString desktopStyle() { return R"(
QWidget { color: #e7eaf3; font-family: 'Noto Sans', 'Noto Sans CJK TC', sans-serif; font-size: 13px; }
QFrame#desktopWindow { background: #1d2434; border: 1px solid #485166; border-radius: 12px; }
QFrame#titleBar { background: #252d40; border-top-left-radius: 12px; border-top-right-radius: 12px; }
QFrame#panel { background: #151b2a; border-bottom: 1px solid #3a4055; }
QFrame#dock, QFrame#launcher { background: #1c2334; border: 1px solid #4a5065; border-radius: 16px; }
QPushButton { background: #30394e; border: 1px solid transparent; border-radius: 7px; padding: 8px 14px; }
QPushButton:hover { background: #46516b; border-color: #677390; }
QPushButton:pressed, QPushButton:checked { background: #566583; }
QPushButton:disabled { color: #818a9e; }
QPushButton#accent { background: #b9c9f6; color: #17223c; font-weight: bold; }
QPushButton#panelButton { background: transparent; font-weight: bold; }
QPushButton#windowControl, QPushButton#closeWindow { padding: 0; background: transparent; }
QPushButton#closeWindow:hover { background: #be626f; }
QPushButton#dockButton { font-size: 22px; padding: 5px; background: #303b53; }
QPushButton#dockButton:checked { border-bottom: 3px solid #bfcef8; }
QLabel#muted { color: #9ca8bf; font-size: 11px; }
QLabel#eyebrow { color: #c5cef7; font-size: 11px; letter-spacing: 2px; }
QLabel#heroTitle { font-size: 32px; font-weight: bold; }
QLabel#heading { font-size: 24px; font-weight: bold; }
QLabel#description { color: #b9c2d5; font-size: 14px; }
QLineEdit, QPlainTextEdit, QTreeView, QListWidget { background: #161d2c; border: 1px solid #37445c; border-radius: 7px; padding: 9px; selection-background-color: #52668c; }
QTreeView { alternate-background-color: #1d2638; }
QTreeView::item { min-height: 30px; }
QHeaderView::section { background: #283247; color: #b9c2d5; border: none; padding: 7px; }
QListWidget::item { padding: 10px; border-radius: 5px; }
QListWidget::item:selected { background: #465a7b; }
QScrollBar:horizontal { background: #1d2638; height: 9px; }
QScrollBar::handle:horizontal { background: #4d5a73; border-radius: 4px; min-width: 22px; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QScrollBar:vertical { background: transparent; width: 9px; }
QScrollBar::handle:vertical { background: #4d5a73; border-radius: 4px; min-height: 22px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QProgressBar { background: #121c2d; border: none; border-radius: 6px; min-height: 34px; text-align: center; }
QProgressBar::chunk { background: #526c98; border-radius: 6px; }
QToolTip { background: #283247; color: #eef2ff; border: 1px solid #566783; }
QMessageBox, QFileDialog { background: #1d2434; }
)";
}
}
