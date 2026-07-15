#include "quickshot/shapes/shape_handle.hpp"

namespace quickshot {

HandlePosition ShapeHandle::oppositePosition(HandlePosition position) noexcept {
  switch (position) {
  case HandlePosition::TopLeft:
    return HandlePosition::BottomRight;
  case HandlePosition::Top:
    return HandlePosition::Bottom;
  case HandlePosition::TopRight:
    return HandlePosition::BottomLeft;
  case HandlePosition::Right:
    return HandlePosition::Left;
  case HandlePosition::BottomRight:
    return HandlePosition::TopLeft;
  case HandlePosition::Bottom:
    return HandlePosition::Top;
  case HandlePosition::BottomLeft:
    return HandlePosition::TopRight;
  case HandlePosition::Left:
    return HandlePosition::Right;
  }

  return position;
}

QPointF ShapeHandle::center(const QRectF& bounds) const {
  switch (position_) {
  case HandlePosition::TopLeft:
    return bounds.topLeft();
  case HandlePosition::Top:
    return {bounds.center().x(), bounds.top()};
  case HandlePosition::TopRight:
    return bounds.topRight();
  case HandlePosition::Right:
    return {bounds.right(), bounds.center().y()};
  case HandlePosition::BottomRight:
    return bounds.bottomRight();
  case HandlePosition::Bottom:
    return {bounds.center().x(), bounds.bottom()};
  case HandlePosition::BottomLeft:
    return bounds.bottomLeft();
  case HandlePosition::Left:
    return {bounds.left(), bounds.center().y()};
  }

  return {};
}

QRectF ShapeHandle::hitRect(const QRectF& bounds, qreal size) const {
  const QPointF handleCenter = center(bounds);
  const qreal halfSize = size / 2.0;
  return {handleCenter.x() - halfSize, handleCenter.y() - halfSize, size, size};
}

Qt::CursorShape ShapeHandle::cursorShape() const noexcept {
  switch (position_) {
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

} // namespace quickshot
