#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace LunaDash {

// Integer window and output geometry. Rect keeps the inclusive right/bottom
// edge convention that compositor layout code and its tests rely on: a
// rectangle at x=10 with width=5 has right() == 14.
class Point {
public:
  constexpr Point() noexcept = default;
  constexpr Point(int x, int y) noexcept : x_(x), y_(y) {}

  constexpr int x() const noexcept { return x_; }
  constexpr int y() const noexcept { return y_; }
  constexpr void setX(int x) noexcept { x_ = x; }
  constexpr void setY(int y) noexcept { y_ = y; }
  constexpr bool isNull() const noexcept { return x_ == 0 && y_ == 0; }
  int manhattanLength() const noexcept { return std::abs(x_) + std::abs(y_); }

  constexpr Point &operator+=(Point other) noexcept {
    x_ += other.x_;
    y_ += other.y_;
    return *this;
  }
  constexpr Point &operator-=(Point other) noexcept {
    x_ -= other.x_;
    y_ -= other.y_;
    return *this;
  }
  friend constexpr Point operator+(Point a, Point b) noexcept {
    return {a.x_ + b.x_, a.y_ + b.y_};
  }
  friend constexpr Point operator-(Point a, Point b) noexcept {
    return {a.x_ - b.x_, a.y_ - b.y_};
  }
  friend constexpr Point operator-(Point a) noexcept { return {-a.x_, -a.y_}; }
  friend constexpr bool operator==(Point a, Point b) noexcept {
    return a.x_ == b.x_ && a.y_ == b.y_;
  }
  friend constexpr bool operator!=(Point a, Point b) noexcept {
    return !(a == b);
  }

private:
  int x_ = 0;
  int y_ = 0;
};

class PointF {
public:
  constexpr PointF() noexcept = default;
  constexpr PointF(double x, double y) noexcept : x_(x), y_(y) {}
  constexpr PointF(Point point) noexcept : x_(point.x()), y_(point.y()) {}

  constexpr double x() const noexcept { return x_; }
  constexpr double y() const noexcept { return y_; }
  constexpr void setX(double x) noexcept { x_ = x; }
  constexpr void setY(double y) noexcept { y_ = y; }
  Point toPoint() const noexcept {
    return {static_cast<int>(std::lround(x_)), static_cast<int>(std::lround(y_))};
  }
  friend constexpr PointF operator+(PointF a, PointF b) noexcept {
    return {a.x_ + b.x_, a.y_ + b.y_};
  }
  friend constexpr PointF operator-(PointF a, PointF b) noexcept {
    return {a.x_ - b.x_, a.y_ - b.y_};
  }
  friend constexpr bool operator==(PointF a, PointF b) noexcept {
    return a.x_ == b.x_ && a.y_ == b.y_;
  }
  friend constexpr bool operator!=(PointF a, PointF b) noexcept {
    return !(a == b);
  }

private:
  double x_ = 0.0;
  double y_ = 0.0;
};

class Size {
public:
  constexpr Size() noexcept = default;
  constexpr Size(int width, int height) noexcept
      : width_(width), height_(height) {}

  constexpr int width() const noexcept { return width_; }
  constexpr int height() const noexcept { return height_; }
  constexpr void setWidth(int width) noexcept { width_ = width; }
  constexpr void setHeight(int height) noexcept { height_ = height; }
  constexpr bool isNull() const noexcept { return width_ == 0 && height_ == 0; }
  constexpr bool isEmpty() const noexcept { return width_ <= 0 || height_ <= 0; }
  constexpr bool isValid() const noexcept { return width_ >= 0 && height_ >= 0; }
  constexpr Size boundedTo(Size other) const noexcept {
    return {std::min(width_, other.width_), std::min(height_, other.height_)};
  }
  constexpr Size expandedTo(Size other) const noexcept {
    return {std::max(width_, other.width_), std::max(height_, other.height_)};
  }
  // Largest size inside bounds that keeps this size's aspect ratio.
  constexpr Size scaledToFit(Size bounds) const noexcept {
    if (width_ == 0 || height_ == 0)
      return bounds;
    const auto scaledWidth = static_cast<std::int64_t>(bounds.height_) *
                             width_ / height_;
    if (scaledWidth <= bounds.width_)
      return {static_cast<int>(scaledWidth), bounds.height_};
    return {bounds.width_, static_cast<int>(static_cast<std::int64_t>(
                               bounds.width_) * height_ / width_)};
  }
  friend constexpr bool operator==(Size a, Size b) noexcept {
    return a.width_ == b.width_ && a.height_ == b.height_;
  }
  friend constexpr bool operator!=(Size a, Size b) noexcept {
    return !(a == b);
  }

private:
  int width_ = -1;
  int height_ = -1;
};

class Rect {
public:
  constexpr Rect() noexcept = default;
  constexpr Rect(int x, int y, int width, int height) noexcept
      : x_(x), y_(y), width_(width), height_(height) {}
  constexpr Rect(Point topLeft, Size size) noexcept
      : x_(topLeft.x()), y_(topLeft.y()), width_(size.width()),
        height_(size.height()) {}

  constexpr int x() const noexcept { return x_; }
  constexpr int y() const noexcept { return y_; }
  constexpr int width() const noexcept { return width_; }
  constexpr int height() const noexcept { return height_; }
  constexpr int left() const noexcept { return x_; }
  constexpr int top() const noexcept { return y_; }
  constexpr int right() const noexcept { return x_ + width_ - 1; }
  constexpr int bottom() const noexcept { return y_ + height_ - 1; }
  constexpr Size size() const noexcept { return {width_, height_}; }
  constexpr Point topLeft() const noexcept { return {x_, y_}; }
  constexpr Point bottomRight() const noexcept { return {right(), bottom()}; }
  constexpr Point center() const noexcept {
    return {static_cast<int>((static_cast<std::int64_t>(x_) + right()) / 2),
            static_cast<int>((static_cast<std::int64_t>(y_) + bottom()) / 2)};
  }

  constexpr bool isNull() const noexcept { return width_ == 0 && height_ == 0; }
  constexpr bool isEmpty() const noexcept { return width_ <= 0 || height_ <= 0; }
  constexpr bool isValid() const noexcept { return width_ > 0 && height_ > 0; }

  constexpr void setX(int x) noexcept { setLeft(x); }
  constexpr void setY(int y) noexcept { setTop(y); }
  constexpr void setWidth(int width) noexcept { width_ = width; }
  constexpr void setHeight(int height) noexcept { height_ = height; }
  constexpr void setSize(Size size) noexcept {
    width_ = size.width();
    height_ = size.height();
  }
  // Edge setters keep the opposite edge fixed.
  constexpr void setLeft(int left) noexcept {
    width_ += x_ - left;
    x_ = left;
  }
  constexpr void setTop(int top) noexcept {
    height_ += y_ - top;
    y_ = top;
  }
  constexpr void setRight(int right) noexcept { width_ = right - x_ + 1; }
  constexpr void setBottom(int bottom) noexcept { height_ = bottom - y_ + 1; }

  constexpr void moveTo(int x, int y) noexcept {
    x_ = x;
    y_ = y;
  }
  constexpr void moveTo(Point point) noexcept { moveTo(point.x(), point.y()); }
  constexpr void moveLeft(int x) noexcept { x_ = x; }
  constexpr void moveTop(int y) noexcept { y_ = y; }
  constexpr void moveCenter(Point point) noexcept {
    x_ = point.x() - (width_ - 1) / 2;
    y_ = point.y() - (height_ - 1) / 2;
  }
  constexpr void translate(int dx, int dy) noexcept {
    x_ += dx;
    y_ += dy;
  }
  constexpr void translate(Point delta) noexcept {
    translate(delta.x(), delta.y());
  }
  constexpr Rect translated(int dx, int dy) const noexcept {
    return {x_ + dx, y_ + dy, width_, height_};
  }
  constexpr Rect translated(Point delta) const noexcept {
    return translated(delta.x(), delta.y());
  }
  constexpr void adjust(int left, int top, int right, int bottom) noexcept {
    x_ += left;
    y_ += top;
    width_ += right - left;
    height_ += bottom - top;
  }
  constexpr Rect adjusted(int left, int top, int right,
                          int bottom) const noexcept {
    auto result = *this;
    result.adjust(left, top, right, bottom);
    return result;
  }

  constexpr bool contains(Point point) const noexcept {
    return isValid() && point.x() >= x_ && point.x() <= right() &&
           point.y() >= y_ && point.y() <= bottom();
  }
  constexpr bool contains(int x, int y) const noexcept {
    return contains(Point{x, y});
  }
  constexpr bool contains(const Rect &other) const noexcept {
    return isValid() && other.isValid() && other.x_ >= x_ &&
           other.right() <= right() && other.y_ >= y_ &&
           other.bottom() <= bottom();
  }
  constexpr bool intersects(const Rect &other) const noexcept {
    return isValid() && other.isValid() && x_ <= other.right() &&
           other.x_ <= right() && y_ <= other.bottom() && other.y_ <= bottom();
  }
  constexpr Rect intersected(const Rect &other) const noexcept {
    if (!intersects(other))
      return {};
    const int left = std::max(x_, other.x_);
    const int top = std::max(y_, other.y_);
    const int rightEdge = std::min(right(), other.right());
    const int bottomEdge = std::min(bottom(), other.bottom());
    return {left, top, rightEdge - left + 1, bottomEdge - top + 1};
  }
  constexpr Rect united(const Rect &other) const noexcept {
    if (isNull())
      return other;
    if (other.isNull())
      return *this;
    const int left = std::min(x_, other.x_);
    const int top = std::min(y_, other.y_);
    const int rightEdge = std::max(right(), other.right());
    const int bottomEdge = std::max(bottom(), other.bottom());
    return {left, top, rightEdge - left + 1, bottomEdge - top + 1};
  }

  friend constexpr bool operator==(const Rect &a, const Rect &b) noexcept {
    return a.x_ == b.x_ && a.y_ == b.y_ && a.width_ == b.width_ &&
           a.height_ == b.height_;
  }
  friend constexpr bool operator!=(const Rect &a, const Rect &b) noexcept {
    return !(a == b);
  }

private:
  int x_ = 0;
  int y_ = 0;
  int width_ = 0;
  int height_ = 0;
};

} // namespace LunaDash
