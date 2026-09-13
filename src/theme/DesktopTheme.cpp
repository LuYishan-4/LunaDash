#include <LuDash/theme/DesktopTheme.h>
namespace LuDash {
QString desktopStyle() { return R"(
QWidget { color: #d6dfdc; font-family: 'Noto Sans Mono', 'Noto Sans CJK TC', monospace; font-size: 13px; }
QFrame#desktopWindow { background: #172426; border: 1px solid #638f94; border-radius: 12px; }
QFrame#titleBar { background: #202e30; border-top-left-radius: 12px; border-top-right-radius: 12px; }
QFrame#panel { background: #151b2a; border-bottom: 1px solid #3a4055; }
QFrame#dock, QFrame#launcher { background: #1c2334; border: 1px solid #4a5065; border-radius: 16px; }
QPushButton { background: #293b3d; border: 1px solid transparent; border-radius: 7px; padding: 8px 14px; }
QPushButton:hover { background: #3e5759; border-color: #638f94; }
QPushButton:pressed, QPushButton:checked { background: #436469; }
QPushButton:disabled { color: #818a9e; }
QPushButton#accent { background: #7dcccf; color: #172628; font-weight: bold; }
QPushButton#panelButton { background: transparent; font-weight: bold; }
QPushButton#windowControl, QPushButton#closeWindow { padding: 0; background: transparent; }
QPushButton#closeWindow:hover { background: #be626f; }
QPushButton#dockButton { font-size: 22px; padding: 5px; background: #303b53; }
QPushButton#dockButton:checked { border-bottom: 3px solid #bfcef8; }
QLabel#muted { color: #96adad; font-size: 11px; }
QLabel#eyebrow { color: #7dcccf; font-size: 11px; letter-spacing: 2px; }
QLabel#heroTitle { font-size: 32px; font-weight: bold; }
QLabel#heading { font-size: 24px; font-weight: bold; }
QLabel#description { color: #adbfbd; font-size: 14px; }
QLineEdit, QPlainTextEdit, QTreeView, QListWidget { background: #142022; border: 1px solid #405f62; border-radius: 7px; padding: 9px; selection-background-color: #436c70; }
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
QMessageBox, QFileDialog { background: #172426; }
)";
}
}
