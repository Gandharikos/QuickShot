#pragma once

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

class ShapeHandle final {
public:
  using Id = std::uint32_t;

  explicit constexpr ShapeHandle(Id id, Qt::CursorShape cursorShape = Qt::ArrowCursor) noexcept
      : id_(id), cursorShape_(cursorShape) {}
  explicit constexpr ShapeHandle(HandlePosition position) noexcept
      : id_(static_cast<Id>(position)), cursorShape_(rectangularCursor(position)) {}

  [[nodiscard]] constexpr Id id() const noexcept { return id_; }
  [[nodiscard]] constexpr Qt::CursorShape cursorShape() const noexcept { return cursorShape_; }
  [[nodiscard]] constexpr bool operator==(const ShapeHandle&) const noexcept = default;

private:
  [[nodiscard]] static constexpr Qt::CursorShape
  rectangularCursor(HandlePosition position) noexcept {
    switch (position) {
    case HandlePosition::TopLeft:
    case HandlePosition::BottomRight:
      return Qt::SizeFDiagCursor;
    case HandlePosition::Top:
    case HandlePosition::Bottom:
      return Qt::SizeVerCursor;
    case HandlePosition::TopRight:
    case HandlePosition::BottomLeft:
      return Qt::SizeBDiagCursor;
    case HandlePosition::Right:
    case HandlePosition::Left:
      return Qt::SizeHorCursor;
    }
    return Qt::ArrowCursor;
  }

  Id id_;
  Qt::CursorShape cursorShape_;
};

} // namespace quickshot
