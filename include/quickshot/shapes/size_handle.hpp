#pragma once

#include <QPointF>
#include <QRectF>
#include <Qt>
#include <cstdint>

namespace quickshot {

enum class HandlePosition : std::uint8_t {
  TopLeft,
  Top,
  TopRight,
  Right,
  BottomRight,
  Bottom,
  BottomLeft,
  Left,
};

class SizeHandle final {
public:
  explicit constexpr SizeHandle(HandlePosition position) noexcept : position_(position) {}

  [[nodiscard]] static HandlePosition oppositePosition(HandlePosition position) noexcept;
  [[nodiscard]] constexpr HandlePosition position() const noexcept { return position_; }
  [[nodiscard]] QPointF center(const QRectF& bounds) const;
  [[nodiscard]] QRectF hitRect(const QRectF& bounds, qreal size) const;
  [[nodiscard]] Qt::CursorShape cursorShape() const noexcept;

private:
  HandlePosition position_;
};

} // namespace quickshot
