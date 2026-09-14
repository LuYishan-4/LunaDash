#include <LuDash/terminal/Terminal.h>

#include <LuDash/configuration/DesktopPreferences.h>
#include <LuDash/localization/Localization.h>

#include <QByteArray>
#include <QClipboard>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QList>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QSettings>
#include <QSocketNotifier>
#include <QStringList>
#include <QSysInfo>
#include <QTemporaryFile>
#include <QTimer>
#include <QVector>
#include <QtMath>
#include <utility>

#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

int qInitResources_terminal_profile();

namespace LuDash {
namespace {

constexpr int kScrollbackLines = 2000;
constexpr int kMinColumns = 2;
constexpr int kMinRows = 2;

struct Cell {
  QChar ch = ' ';
  QColor fg;
  QColor bg;
  bool bold = false;
  bool reverse = false;
};

struct Palette {
  QColor background;
  QColor foreground;
  QColor accent;
  QVector<QColor> ansi; // 16 base colours; the blue entries follow the accent.
  int opacity = 84;     // window background opacity, 40..100.
};

QColor parseColor(const QString &value, const QColor &fallback) {
  const QColor parsed(value);
  return parsed.isValid() ? parsed : fallback;
}

QColor lighten(const QColor &color, qreal amount) {
  return QColor(qMin(255, int(color.red() + (255 - color.red()) * amount)),
                qMin(255, int(color.green() + (255 - color.green()) * amount)),
                qMin(255, int(color.blue() + (255 - color.blue()) * amount)));
}

// Fish accepts 24-bit colours as bare RRGGBB (no '#'); QColor::name() returns
// "#RRGGBB", so strip the leading hash before exporting the prompt gradient.
QByteArray hexRgb(const QColor &color) {
  return color.name().toLatin1().mid(1);
}

// Fades a colour towards a neutral grey so the prompt can draw an
// accent -> muted -> grey background while still following the theme.
QColor desaturate(const QColor &color, qreal amount) {
  float h, s, l, a;
  color.getHslF(&h, &s, &l, &a);
  s = qMax(0.0f, s * float(1.0 - amount));
  QColor result;
  result.setHslF(h, s, l, a);
  return result;
}

QVector<QColor> defaultAnsi(const QColor &accent, const QColor &foreground,
                            const QColor &background) {
  // The blue slots (indices 4 and 12) use the desktop accent so directory
  // listings and other blue terminal output follow the theme. Everything else
  // keeps a readable, calm palette built around the shell colours.
  QVector<QColor> ansi(16);
  ansi[0] = background.darker(118);
  ansi[1] = QColor("#f2b8b5");
  ansi[2] = QColor("#a7d8a9");
  ansi[3] = QColor("#e6c07b");
  ansi[4] = accent;
  ansi[5] = QColor("#d3bfe6");
  ansi[6] = QColor("#7dcccf");
  ansi[7] = foreground;
  for (int i = 0; i < 8; ++i)
    ansi[8 + i] = lighten(ansi[i], 0.34);
  ansi[12] = lighten(accent, 0.4);
  return ansi;
}

Palette loadPalette() {
  const auto preferences = desktopPreferences();
  const QColor accent(
      parseColor(preferences.value("accent").toString(), QColor("#9ccbfb")));
  QSettings settings;
  const QColor foreground(parseColor(
      settings.value("terminal/foreground").toString(), QColor("#e2e9f1")));
  const QColor background(parseColor(
      settings.value("terminal/background").toString(), QColor("#0d1218")));
  Palette palette;
  palette.background = background;
  palette.foreground = foreground;
  palette.accent = accent;
  palette.ansi = defaultAnsi(accent, foreground, background);
  palette.opacity =
      qBound(40, settings.value("terminal/opacity", 84).toInt(), 100);
  return palette;
}

// Approximates the xterm 256-colour cube so Fish and `ls --color` keep their
// distinction without shipping a full colour table in every cell.
QColor from256(int index, const QVector<QColor> &ansi) {
  if (index < 16)
    return ansi[index];
  if (index < 232) {
    const int value = index - 16;
    const int levels[6] = {0, 95, 135, 175, 215, 255};
    return QColor(levels[value / 36], levels[(value / 6) % 6],
                  levels[value % 6]);
  }
  const int value = 8 + (index - 232) * 10;
  return QColor(value, value, value);
}

int utf8SequenceLength(unsigned char lead) {
  if (lead < 0x80)
    return 1;
  if (lead < 0xC2)
    return 0;
  if (lead < 0xE0)
    return 2;
  if (lead < 0xF0)
    return 3;
  if (lead < 0xF5)
    return 4;
  return 0;
}

class Terminal : public QWidget {
  Q_OBJECT
public:
  explicit Terminal(QWidget *parent = nullptr) : QWidget(parent) {
    qInitResources_terminal_profile();
    palette_ = loadPalette();

    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
    setMouseTracking(true);

    QSettings settings;
    const QString family =
        settings.value("terminal/font", "JetBrainsMono Nerd Font Mono")
            .toString();
    font_ = QFont(family, settings.value("terminal/fontSize", 11).toInt());
    font_.setStyleHint(QFont::Monospace);
    font_.setFixedPitch(true);
    setFont(font_);

    cellWidth_ = qMax(6, fontMetrics().horizontalAdvance(QLatin1Char('M')));
    cellHeight_ = qMax(10, fontMetrics().height());

    rows_ = kMinRows;
    columns_ = kMinColumns;
    allocateGrid();

    cursorTimer_ = new QTimer(this);
    cursorTimer_->setInterval(530);
    connect(cursorTimer_, &QTimer::timeout, this, [this] {
      cursorVisible_ = !cursorVisible_;
      update(cursorRect());
    });
    cursorTimer_->start();

    resizeTimer_ = new QTimer(this);
    resizeTimer_->setSingleShot(true);
    resizeTimer_->setInterval(80);
    connect(resizeTimer_, &QTimer::timeout, this, [this] { sendSize(); });
  }

  ~Terminal() override {
    if (masterFd_ >= 0) {
      ::close(masterFd_);
      masterFd_ = -1;
    }
    if (pid_ > 0) {
      ::kill(pid_, SIGHUP);
      int status = 0;
      ::waitpid(pid_, &status, WNOHANG);
    }
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing, false);
    painter.setFont(font_);

    const qreal alpha = palette_.opacity / 100.0;
    const QColor surface(palette_.background.red(), palette_.background.green(),
                         palette_.background.blue(), int(alpha * 255));
    QPainterPath backdrop;
    backdrop.addRoundedRect(rect(), 14, 14);
    painter.fillPath(backdrop, surface);

    const int baseline = fontMetrics().ascent();
    for (int row = 0; row < rows_; ++row) {
      const int y = row * cellHeight_;
      for (int column = 0; column < columns_; ++column) {
        const Cell &cell = grid_[row][column];
        QColor fg = cell.fg.isValid() ? cell.fg : palette_.foreground;
        QColor bg = cell.bg;
        if (cell.reverse) {
          if (!bg.isValid())
            bg = palette_.foreground;
          std::swap(fg, bg);
        }
        if (bg.isValid())
          painter.fillRect(column * cellWidth_, y, cellWidth_, cellHeight_, bg);
        if (isSelected(row, column)) {
          QColor selection = palette_.accent;
          selection.setAlpha(110);
          painter.fillRect(column * cellWidth_, y, cellWidth_, cellHeight_,
                           selection);
        }
        painter.setPen(fg);
        if (cell.bold) {
          QFont boldFont = font_;
          boldFont.setBold(true);
          painter.setFont(boldFont);
        }
        painter.drawText(column * cellWidth_, y + baseline, cell.ch);
        if (cell.bold)
          painter.setFont(font_);
      }
    }

    if (cursorVisible_ && cursorEnabled_)
      painter.fillRect(cursorRect(), palette_.accent);
  }

  void keyPressEvent(QKeyEvent *event) override {
    const auto mods = event->modifiers();
    if (mods & Qt::ControlModifier && mods & Qt::ShiftModifier) {
      if (event->key() == Qt::Key_C) {
        copySelection();
        event->accept();
        return;
      }
      if (event->key() == Qt::Key_V) {
        pasteClipboard();
        event->accept();
        return;
      }
    }

    QByteArray bytes;
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
      bytes = "\r";
      break;
    case Qt::Key_Backspace:
      bytes = "\x7f";
      break;
    case Qt::Key_Tab:
      bytes = "\t";
      break;
    case Qt::Key_Escape:
      bytes = "\x1b";
      break;
    case Qt::Key_Up:
      bytes = "\x1b[A";
      break;
    case Qt::Key_Down:
      bytes = "\x1b[B";
      break;
    case Qt::Key_Right:
      bytes = "\x1b[C";
      break;
    case Qt::Key_Left:
      bytes = "\x1b[D";
      break;
    case Qt::Key_Home:
      bytes = "\x1b[H";
      break;
    case Qt::Key_End:
      bytes = "\x1b[F";
      break;
    case Qt::Key_Delete:
      bytes = "\x1b[3~";
      break;
    case Qt::Key_PageUp:
      bytes = "\x1b[5~";
      break;
    case Qt::Key_PageDown:
      bytes = "\x1b[6~";
      break;
    case Qt::Key_Insert:
      bytes = "\x1b[2~";
      break;
    default:
      if (mods & Qt::ControlModifier) {
        const int key = event->key();
        if (key >= Qt::Key_A && key <= Qt::Key_Z)
          bytes = QByteArray(1, char(key - Qt::Key_A + 1));
      } else if (!(mods & (Qt::AltModifier | Qt::MetaModifier))) {
        bytes = event->text().toUtf8();
      }
      break;
    }
    if (!bytes.isEmpty()) {
      clearSelection();
      writeToPty(bytes);
    }
    event->accept();
  }

  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton) {
      selecting_ = true;
      selectionAnchor_ = cellAt(event->pos());
      selectionEnd_ = selectionAnchor_;
      update();
      event->accept();
      return;
    }
    QWidget::mousePressEvent(event);
  }

  void mouseMoveEvent(QMouseEvent *event) override {
    if (selecting_ && (event->buttons() & Qt::LeftButton)) {
      selectionEnd_ = cellAt(event->pos());
      update();
      event->accept();
      return;
    }
    QWidget::mouseMoveEvent(event);
  }

  void mouseReleaseEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton && selecting_) {
      selecting_ = false;
      selectionEnd_ = cellAt(event->pos());
      update();
      event->accept();
      return;
    }
    QWidget::mouseReleaseEvent(event);
  }

  void resizeEvent(QResizeEvent *) override { resizeTimer_->start(); }

  void focusInEvent(QFocusEvent *event) override {
    QWidget::focusInEvent(event);
    cursorVisible_ = true;
    update(cursorRect());
  }

private:
  bool startSession() {
    masterFd_ = ::posix_openpt(O_RDWR | O_NOCTTY);
    if (masterFd_ < 0)
      return false;
    if (::grantpt(masterFd_) != 0 || ::unlockpt(masterFd_) != 0)
      return false;
    char slaveName[128];
    if (::ptsname_r(masterFd_, slaveName, sizeof(slaveName)) != 0)
      return false;
    const int slave = ::open(slaveName, O_RDWR | O_NOCTTY);
    if (slave < 0)
      return false;

    QString profilePath;
    QFile resource(QStringLiteral(":/LuDash/data/terminal/ludash.fish"));
    if (resource.open(QIODevice::ReadOnly)) {
      profileFile_.setFileTemplate(
          QDir::temp().filePath(QStringLiteral("lunadash-fish-XXXXXX")));
      if (profileFile_.open()) {
        profileFile_.write(resource.readAll());
        profileFile_.flush();
        profilePath = profileFile_.fileName();
      }
    }

    // Export the accent, a desaturated "muted" shade, and a neutral grey so
    // the Fish prompt fades accent -> muted -> grey while still following the
    // desktop theme.
    const QByteArray acAccent = hexRgb(palette_.accent);
    const QByteArray acMuted = hexRgb(desaturate(palette_.accent, 0.55));
    const QByteArray acGray = hexRgb(desaturate(palette_.accent, 1.0));

    pid_ = ::fork();
    if (pid_ == 0) {
      ::setsid();
      ::ioctl(slave, TIOCSCTTY, 0);
      ::dup2(slave, 0);
      ::dup2(slave, 1);
      ::dup2(slave, 2);
      if (slave > 2)
        ::close(slave);
      ::setenv("TERM", "xterm-256color", 1);
      ::setenv("LUDASH_TERM_AC_ACCENT", acAccent.constData(), 1);
      ::setenv("LUDASH_TERM_AC_MUTED", acMuted.constData(), 1);
      ::setenv("LUDASH_TERM_AC_GRAY", acGray.constData(), 1);
      if (profilePath.isEmpty()) {
        ::execlp("fish", "fish", "--interactive", static_cast<char *>(nullptr));
      } else {
        const QByteArray source =
            QByteArray("source '") + profilePath.toUtf8() + "'";
        ::execlp("fish", "fish", "--interactive", "-C", source.constData(),
                 static_cast<char *>(nullptr));
      }
      ::_exit(127);
    }
    ::close(slave);
    const int flags = ::fcntl(masterFd_, F_GETFL);
    ::fcntl(masterFd_, F_SETFL, flags | O_NONBLOCK);
    notifier_ = new QSocketNotifier(masterFd_, QSocketNotifier::Read, this);
    connect(notifier_, &QSocketNotifier::activated, this,
            [this](QSocketDescriptor, QSocketNotifier::Type) { drain(); });
    return pid_ > 0;
  }

  static void resizeCells(QVector<QVector<Cell>> &grid, int rows, int columns) {
    QVector<QVector<Cell>> next(rows, QVector<Cell>(columns, Cell{}));
    for (int row = 0; row < qMin(rows, grid.size()); ++row)
      for (int column = 0; column < qMin(columns, grid[row].size()); ++column)
        next[row][column] = grid[row][column];
    grid = std::move(next);
  }

  void allocateGrid() {
    resizeCells(grid_, rows_, columns_);
    if (alternateScreen_)
      resizeCells(normalGrid_, rows_, columns_);
  }

  void writeDefaultContent() {
    // The interactive Fish profile is responsible for drawing the prompt and
    // any welcome banner; nothing extra is written here.
  }

  void appendLine(const QString &text) {
    scrollBack();
    for (int column = 0; column < columns_; ++column)
      grid_[rows_ - 1][column] = Cell{};
    int column = 0;
    for (const QChar &ch : text) {
      if (column < columns_)
        grid_[rows_ - 1][column] =
            Cell{ch, palette_.foreground, QColor(), false, false};
      column++;
    }
    cursor_ = QPoint(0, rows_ - 1);
    update();
  }

  void putChar(const QChar &ch) {
    if (cursor_.y() >= rows_)
      scrollUp();
    grid_[cursor_.y()][cursor_.x()] =
        Cell{ch, currentFg_, currentBg_, bold_, reverse_};
    if (cursor_.x() + 1 < columns_)
      cursor_.rx()++;
    else {
      cursor_.rx() = 0;
      if (cursor_.y() + 1 < rows_)
        cursor_.ry()++;
      else
        scrollUp();
    }
    update(cursorRect());
  }

  void lineFeed() {
    if (cursor_.y() + 1 < rows_)
      cursor_.ry()++;
    else
      scrollUp();
  }

  void scrollUp() {
    clearSelection();
    if (!alternateScreen_) {
      scrollback_.append(grid_.first());
      while (scrollback_.size() > kScrollbackLines)
        scrollback_.pop_front();
    }
    for (int row = 1; row < rows_; ++row)
      grid_[row - 1] = grid_[row];
    grid_[rows_ - 1].fill(Cell{});
  }

  void scrollBack() {
    scrollback_.append(grid_.first());
    while (scrollback_.size() > kScrollbackLines)
      scrollback_.pop_front();
  }

  void eraseLine(int mode) {
    const int row = cursor_.y();
    int begin = 0, end = columns_ - 1;
    if (mode == 0) {
      begin = cursor_.x();
    } else if (mode == 1) {
      end = cursor_.x();
    }
    for (int column = begin; column <= end; ++column)
      grid_[row][column] = Cell{};
    update();
  }

  void eraseDisplay(int mode) {
    if (mode == 0) {
      eraseLine(0);
      for (int row = cursor_.y() + 1; row < rows_; ++row)
        grid_[row].fill(Cell{});
    } else if (mode == 1) {
      for (int row = 0; row < cursor_.y(); ++row)
        grid_[row].fill(Cell{});
      eraseLine(1);
    } else if (mode == 2 || mode == 3) {
      for (auto &row : grid_)
        row.fill(Cell{});
      cursor_ = QPoint(0, 0);
    }
    update();
  }

  void enterAlternateScreen() {
    if (alternateScreen_)
      return;
    normalGrid_ = grid_;
    for (auto &row : grid_)
      row.fill(Cell{});
    alternateScreen_ = true;
    cursor_ = QPoint(0, 0);
    update();
  }

  void leaveAlternateScreen() {
    if (!alternateScreen_)
      return;
    grid_ = std::move(normalGrid_);
    normalGrid_.clear();
    alternateScreen_ = false;
    cursor_.setX(qMin(cursor_.x(), columns_ - 1));
    cursor_.setY(qMin(cursor_.y(), rows_ - 1));
    update();
  }

  void handlePrivateMode(const QString &parameters, char final) {
    if (final != 'h' && final != 'l')
      return;
    const bool enable = (final == 'h');
    bool ok = false;
    const int mode = parameters.toInt(&ok);
    if (!ok)
      return;
    switch (mode) {
    case 25: // cursor visibility
      cursorEnabled_ = enable;
      break;
    case 47: // alternate screen buffer
      if (enable)
        enterAlternateScreen();
      else
        leaveAlternateScreen();
      break;
    case 1049: // alternate screen buffer + cursor save/restore
      if (enable) {
        savedCursor_ = cursor_;
        enterAlternateScreen();
      } else {
        leaveAlternateScreen();
        cursor_ = savedCursor_;
      }
      break;
    default:
      break;
    }
  }

  void moveCursor(int rows, int columns) {
    cursor_.setY(qBound(0, cursor_.y() + rows, rows_ - 1));
    cursor_.setX(qBound(0, cursor_.x() + columns, columns_ - 1));
  }

  void setCursor(int row, int column) {
    cursor_.setX(qBound(0, column - 1, columns_ - 1));
    cursor_.setY(qBound(0, row - 1, rows_ - 1));
  }

  QRect cursorRect() const {
    const int thickness = qMax(2, cellHeight_ / 9);
    return QRect(cursor_.x() * cellWidth_,
                 cursor_.y() * cellHeight_ + cellHeight_ - thickness,
                 cellWidth_, thickness);
  }

  QPoint cellAt(const QPoint &pos) const {
    const int column = qBound(0, pos.x() / cellWidth_, columns_ - 1);
    const int row = qBound(0, pos.y() / cellHeight_, rows_ - 1);
    return QPoint(column, row);
  }

  QPoint selectionStart() const {
    if (selectionAnchor_.y() < selectionEnd_.y() ||
        (selectionAnchor_.y() == selectionEnd_.y() &&
         selectionAnchor_.x() <= selectionEnd_.x()))
      return selectionAnchor_;
    return selectionEnd_;
  }

  QPoint selectionFinish() const {
    if (selectionAnchor_.y() > selectionEnd_.y() ||
        (selectionAnchor_.y() == selectionEnd_.y() &&
         selectionAnchor_.x() > selectionEnd_.x()))
      return selectionAnchor_;
    return selectionEnd_;
  }

  QChar cellChar(int row, int column) const {
    const QChar ch = grid_[row][column].ch;
    return ch.isNull() ? QLatin1Char(' ') : ch;
  }

  bool isSelected(int row, int column) const {
    if (selectionAnchor_ == selectionEnd_)
      return false;
    const QPoint start = selectionStart();
    const QPoint finish = selectionFinish();
    if (row < start.y() || row > finish.y())
      return false;
    if (row == start.y() && row == finish.y())
      return column >= start.x() && column <= finish.x();
    if (row == start.y())
      return column >= start.x();
    if (row == finish.y())
      return column <= finish.x();
    return true;
  }

  QString selectedText() const {
    if (selectionAnchor_ == selectionEnd_)
      return QString();
    const QPoint start = selectionStart();
    const QPoint finish = selectionFinish();
    QStringList lines;
    for (int row = start.y(); row <= finish.y(); ++row) {
      const int firstColumn = (row == start.y()) ? start.x() : 0;
      int lastColumn = (row == finish.y()) ? finish.x() : (columns_ - 1);
      while (lastColumn >= firstColumn &&
             cellChar(row, lastColumn) == QLatin1Char(' '))
        --lastColumn;
      QString line;
      if (lastColumn >= firstColumn)
        for (int column = firstColumn; column <= lastColumn; ++column)
          line += cellChar(row, column);
      lines << line;
    }
    while (!lines.isEmpty() && lines.last().isEmpty())
      lines.removeLast();
    return lines.join(QLatin1Char('\n'));
  }

  void copySelection() {
    const QString text = selectedText();
    if (!text.isEmpty())
      QGuiApplication::clipboard()->setText(text);
  }

  void pasteClipboard() {
    const QString text = QGuiApplication::clipboard()->text();
    if (text.isEmpty())
      return;
    clearSelection();
    writeToPty(text.toUtf8());
  }

  void clearSelection() {
    selecting_ = false;
    if (selectionAnchor_ == selectionEnd_)
      return;
    selectionAnchor_ = QPoint();
    selectionEnd_ = QPoint();
    update();
  }

  void sendSize() {
    const int usableColumns = qMax(kMinColumns, width() / cellWidth_);
    const int usableRows = qMax(kMinRows, height() / cellHeight_);
    const bool first = !sized_;
    if (!first && usableColumns == columns_ && usableRows == rows_)
      return;
    columns_ = usableColumns;
    rows_ = usableRows;
    allocateGrid();
    cursor_ =
        QPoint(qMin(cursor_.x(), columns_ - 1), qMin(cursor_.y(), rows_ - 1));
    if (first) {
      sized_ = true;
      writeDefaultContent();
      if (!startSession())
        appendLine(LuDash::translate("Could not start the Fish shell. "
                                     "Install fish to use the terminal."));
    }
    if (masterFd_ < 0)
      return;
    struct winsize size{};
    size.ws_row = static_cast<unsigned short>(rows_);
    size.ws_col = static_cast<unsigned short>(columns_);
    size.ws_xpixel = static_cast<unsigned short>(width());
    size.ws_ypixel = static_cast<unsigned short>(height());
    ::ioctl(masterFd_, TIOCSWINSZ, &size);
    if (pid_ > 0)
      ::kill(pid_, SIGWINCH);
    update();
  }

  void writeToPty(const QByteArray &bytes) {
    if (masterFd_ < 0)
      return;
    const ssize_t written = ::write(masterFd_, bytes.constData(), bytes.size());
    Q_UNUSED(written);
  }

  void drain() {
    if (masterFd_ < 0)
      return;
    char buffer[8192];
    for (;;) {
      const ssize_t count = ::read(masterFd_, buffer, sizeof(buffer));
      if (count == 0) {
        handleExit();
        break;
      }
      if (count < 0)
        break;
      feed(QByteArray(buffer, int(count)));
    }
  }

  void handleExit() {
    if (masterFd_ >= 0) {
      ::close(masterFd_);
      masterFd_ = -1;
    }
    if (auto *top = window())
      top->close();
  }

  void feed(const QByteArray &bytes) {
    for (const unsigned char byte : bytes)
      feedByte(byte);
  }

  void feedByte(unsigned char byte) {
    switch (state_) {
    case State::Ground:
      if (byte == 0x1b) {
        state_ = State::Escape;
      } else if (byte == '\r') {
        cursor_.rx() = 0;
        update(cursorRect());
      } else if (byte == '\n') {
        cursor_.rx() = 0;
        lineFeed();
      } else if (byte == '\b') {
        if (cursor_.x() > 0)
          cursor_.rx()--;
        update(cursorRect());
      } else if (byte == '\t') {
        do {
          putChar(QLatin1Char(' '));
        } while (cursor_.x() % 8 != 0 && cursor_.x() < columns_ - 1);
      } else if (byte == 0x07) {
        // Bell: ignored.
      } else if (byte >= 0x20) {
        if (byte < 0x80) {
          putChar(QChar(byte));
        } else {
          utf8_.append(char(byte));
          if (utf8_.size() == 1)
            utf8Expected_ = utf8SequenceLength(byte);
          if (utf8Expected_ > 0 && utf8_.size() == utf8Expected_) {
            flushUtf8();
            utf8Expected_ = 0;
          } else if (utf8Expected_ == 0) {
            utf8_.clear();
          }
        }
      }
      break;
    case State::Escape:
      if (byte == '[') {
        state_ = State::Csi;
        csi_.clear();
      } else if (byte == ']') {
        state_ = State::Osc;
        osc_.clear();
      } else if (byte == 'P') {
        // Device Control String (XTGETTCAP queries, SIXEL, tmux passthrough).
        // The terminal does not answer these; discard the payload.
        state_ = State::Dcs;
      } else if (byte == '7') {
        savedCursor_ = cursor_;
        state_ = State::Ground;
      } else if (byte == '8') {
        cursor_ = savedCursor_;
        state_ = State::Ground;
        update(cursorRect());
      } else if (byte == 'c') {
        for (auto &row : grid_)
          row.fill(Cell{});
        cursor_ = QPoint(0, 0);
        state_ = State::Ground;
        update();
      } else if (byte == 'D') {
        lineFeed();
        state_ = State::Ground;
      } else {
        state_ = State::Ground;
      }
      break;
    case State::Csi:
      csi_.append(char(byte));
      if (byte >= 0x40 && byte <= 0x7e) {
        handleCsi();
        state_ = State::Ground;
      }
      break;
    case State::Osc:
      if (byte == 0x07) {
        handleOsc();
        state_ = State::Ground;
      } else if (byte == 0x1b) {
        state_ = State::OscEscape;
      } else {
        osc_.append(char(byte));
      }
      break;
    case State::OscEscape:
      if (byte == '\\') {
        handleOsc();
        state_ = State::Ground;
      } else if (byte == 0x07) {
        handleOsc();
        state_ = State::Ground;
      } else {
        osc_.append(char(byte));
        state_ = State::Osc;
      }
      break;
    case State::Dcs:
      if (byte == 0x1b) {
        state_ = State::DcsEscape;
      } else if (byte == 0x07) {
        state_ = State::Ground;
      }
      break;
    case State::DcsEscape:
      if (byte == '\\' || byte == 0x07) {
        state_ = State::Ground;
      } else {
        state_ = State::Dcs;
      }
      break;
    }
  }

  void flushUtf8() {
    const QString text = QString::fromUtf8(utf8_);
    for (const QChar &ch : text)
      putChar(ch);
    utf8_.clear();
  }

  void handleCsi() {
    if (csi_.isEmpty())
      return;
    const char final = csi_.at(csi_.size() - 1);
    QString parameters = QString::fromLatin1(csi_);
    parameters.chop(1);
    if (parameters.startsWith('?')) {
      handlePrivateMode(parameters.mid(1), final);
      return;
    }
    QList<int> values;
    const QStringList parts = parameters.split(';');
    for (const QString &part : parts) {
      bool ok = false;
      const int value = part.toInt(&ok);
      values << (ok ? value : 0);
    }
    const int first = values.value(0, 1);
    switch (final) {
    case 'A':
      moveCursor(-qMax(1, first), 0);
      break;
    case 'B':
      moveCursor(qMax(1, first), 0);
      break;
    case 'C':
      moveCursor(0, qMax(1, first));
      break;
    case 'D':
      moveCursor(0, -qMax(1, first));
      break;
    case 'E':
      lineFeed();
      cursor_.rx() = 0;
      break;
    case 'G':
    case '`':
      cursor_.rx() = qBound(0, first - 1, columns_ - 1);
      break;
    case 'H':
    case 'f':
      setCursor(values.value(0, 1), values.value(1, 1));
      break;
    case 'J':
      eraseDisplay(values.value(0, 0));
      break;
    case 'K':
      eraseLine(values.value(0, 0));
      break;
    case 'm':
      applySgr(values);
      break;
    case 's':
      savedCursor_ = cursor_;
      break;
    case 'u':
      cursor_ = savedCursor_;
      break;
    case 'c':
      handleDeviceAttributes(csi_);
      break;
    case 'n':
      handleDsr(values);
      break;
    case 'h':
    case 'l':
    case 'r':
    case 't':
      break;
    default:
      break;
    }
    update();
  }

  // Answers the terminal queries Fish sends on startup so its prompt (and the
  // right prompt it derives from the cursor position) can be laid out without
  // guessing. Without the cursor-position reply, typing a key lands the cursor
  // on a stale line and Fish redraws history.
  void handleDsr(const QList<int> &values) {
    switch (values.value(0, 0)) {
    case 5:
      writeToPty(QByteArrayLiteral("\x1b[0n"));
      break;
    case 6:
      writeToPty("\x1b[" + QByteArray::number(cursor_.y() + 1) + ";" +
                 QByteArray::number(cursor_.x() + 1) + "R");
      break;
    default:
      break;
    }
  }

  void handleDeviceAttributes(const QByteArray &csi) {
    const QString params = QString::fromLatin1(csi);
    if (params.contains('>')) {
      if (params.endsWith('q'))
        writeToPty(QByteArrayLiteral("\x1bP>|LunaDash(0.1.0)\x1b\\"));
      else
        writeToPty(QByteArrayLiteral("\x1b[>0;276;0c"));
    } else {
      writeToPty(QByteArrayLiteral("\x1b[?1;2c"));
    }
  }

  void applySgr(const QList<int> &values) {
    for (int i = 0; i < values.size(); ++i) {
      const int value = values[i];
      if (value == 0) {
        currentFg_ = QColor();
        currentBg_ = QColor();
        bold_ = false;
        reverse_ = false;
      } else if (value == 1) {
        bold_ = true;
      } else if (value == 22) {
        bold_ = false;
      } else if (value == 7) {
        reverse_ = true;
      } else if (value == 27) {
        reverse_ = false;
      } else if (value == 39) {
        currentFg_ = QColor();
      } else if (value == 49) {
        currentBg_ = QColor();
      } else if (value >= 30 && value <= 37) {
        currentFg_ = palette_.ansi[value - 30];
      } else if (value >= 40 && value <= 47) {
        currentBg_ = palette_.ansi[value - 40];
      } else if (value >= 90 && value <= 97) {
        currentFg_ = palette_.ansi[8 + value - 90];
      } else if (value >= 100 && value <= 107) {
        currentBg_ = palette_.ansi[8 + value - 100];
      } else if (value == 38 || value == 48) {
        QColor *target = value == 38 ? &currentFg_ : &currentBg_;
        if (i + 1 < values.size() && values[i + 1] == 5 &&
            i + 2 < values.size()) {
          *target = from256(values[i + 2], palette_.ansi);
          i += 2;
        } else if (i + 1 < values.size() && values[i + 1] == 2 &&
                   i + 4 < values.size()) {
          *target = QColor(values[i + 2], values[i + 3], values[i + 4]);
          i += 4;
        }
      }
    }
  }

  void handleOsc() {
    // OSC 0 / 2 set the window title; everything else is ignored because the
    // palette is owned by the desktop theme and its optional overrides.
    const int separator = osc_.indexOf(';');
    if (separator > 0) {
      const int command = osc_.left(separator).toInt();
      if (command == 0 || command == 2) {
        const QString title = QString::fromUtf8(osc_.mid(separator + 1));
        if (!title.trimmed().isEmpty()) {
          if (auto *windowWidget = window())
            windowWidget->setWindowTitle(title + QStringLiteral(" — LunaDash"));
        }
      }
    }
  }

  enum class State { Ground, Escape, Csi, Osc, OscEscape, Dcs, DcsEscape };

  Palette palette_;
  QFont font_;
  int cellWidth_ = 8;
  int cellHeight_ = 16;
  int rows_ = kMinRows;
  int columns_ = kMinColumns;
  QVector<QVector<Cell>> grid_;
  QList<QVector<Cell>> scrollback_;
  QVector<QVector<Cell>> normalGrid_;
  bool alternateScreen_ = false;

  QPoint cursor_{0, 0};
  QPoint savedCursor_{0, 0};
  QColor currentFg_;
  QColor currentBg_;
  bool bold_ = false;
  bool reverse_ = false;
  bool cursorVisible_ = true;
  bool cursorEnabled_ = true;
  bool sized_ = false;
  bool selecting_ = false;
  QPoint selectionAnchor_;
  QPoint selectionEnd_;

  State state_ = State::Ground;
  QByteArray csi_;
  QByteArray osc_;
  QByteArray utf8_;
  int utf8Expected_ = 0;

  int masterFd_ = -1;
  pid_t pid_ = -1;
  QTemporaryFile profileFile_;
  QSocketNotifier *notifier_ = nullptr;
  QTimer *cursorTimer_ = nullptr;
  QTimer *resizeTimer_ = nullptr;
};

} // namespace

QWidget *createTerminal() { return new Terminal; }

} // namespace LuDash

#include "Terminal.moc"
