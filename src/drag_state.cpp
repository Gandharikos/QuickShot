#include "quickshot/drag_state.hpp"

#include "quickshot/shapes/shape.hpp"

#include <QtMath>
#include <algorithm>
#include <cmath>

namespace quickshot {
namespace {

constexpr qreal minimumShapeSize = 1.0;

QTransform rotationTransform(const QRectF& bounds, qreal degrees) {
  const QPointF center = bounds.center();
  QTransform transformation;
  transformation.translate(center.x(), center.y());
  transformation.rotate(degrees);
  transformation.translate(-center.x(), -center.y());
  return transformation;
}

qreal mouseAngle(const QPointF& center, const QPointF& point) {
  return qRadiansToDegrees(std::atan2(point.y() - center.y(), point.x() - center.x()));
}

QRectF constrainedMove(const QRectF& bounds, const QPointF& offset, const QRectF& limits) {
  const qreal horizontalOffset =
      std::clamp(offset.x(), limits.left() - bounds.left(), limits.right() - bounds.right());
  const qreal verticalOffset =
      std::clamp(offset.y(), limits.top() - bounds.top(), limits.bottom() - bounds.bottom());
  return bounds.translated(horizontalOffset, verticalOffset);
}

QRectF resizedBounds(const QRectF& bounds, HandlePosition handle, const QPointF& point,
                     const QRectF& limits) {
  qreal left = bounds.left();
  qreal top = bounds.top();
  qreal right = bounds.right();
  qreal bottom = bounds.bottom();

  switch (handle) {
  case HandlePosition::TopLeft:
  case HandlePosition::Left:
  case HandlePosition::BottomLeft:
    left = std::clamp(point.x(), limits.left(), right - minimumShapeSize);
    break;
  case HandlePosition::TopRight:
  case HandlePosition::Right:
  case HandlePosition::BottomRight:
    right = std::clamp(point.x(), left + minimumShapeSize, limits.right());
    break;
  case HandlePosition::Top:
  case HandlePosition::Bottom:
    break;
  }

  switch (handle) {
  case HandlePosition::TopLeft:
  case HandlePosition::Top:
  case HandlePosition::TopRight:
    top = std::clamp(point.y(), limits.top(), bottom - minimumShapeSize);
    break;
  case HandlePosition::BottomRight:
  case HandlePosition::Bottom:
  case HandlePosition::BottomLeft:
    bottom = std::clamp(point.y(), top + minimumShapeSize, limits.bottom());
    break;
  case HandlePosition::Right:
  case HandlePosition::Left:
    break;
  }

  return {QPointF{left, top}, QPointF{right, bottom}};
}

} // namespace

DragState::DragState(Qt::MouseButton completionButton) noexcept
    : completionButton_(completionButton) {}

DragResult DragState::finish() const noexcept { return DragResult::KeepShape; }

std::optional<HandlePosition> DragState::activeHandle() const noexcept { return std::nullopt; }

Qt::MouseButton DragState::completionButton() const noexcept { return completionButton_; }

CreateState::CreateState(Shape& shape, const QPointF& origin, const QRectF& limits) noexcept
    : DragState(Qt::LeftButton), shape_(shape), origin_(origin), limits_(limits) {}

void CreateState::update(const QPointF& point) {
  const QPointF boundedPoint{std::clamp(point.x(), limits_.left(), limits_.right()),
                             std::clamp(point.y(), limits_.top(), limits_.bottom())};
  shape_.setBoundingRect(QRectF{origin_, boundedPoint}.normalized());
}

DragResult CreateState::finish() const noexcept {
  const QRectF bounds = shape_.boundingRect();
  return bounds.width() < minimumShapeSize || bounds.height() < minimumShapeSize
             ? DragResult::RemoveShape
             : DragResult::KeepShape;
}

MoveState::MoveState(Shape& shape, const QPointF& origin, const QRectF& limits)
    : DragState(Qt::LeftButton), shape_(shape), origin_(origin),
      initialBounds_(shape.boundingRect()), limits_(limits) {}

void MoveState::update(const QPointF& point) {
  shape_.setBoundingRect(constrainedMove(initialBounds_, point - origin_, limits_));
}

ResizeState::ResizeState(Shape& shape, HandlePosition handle, const QRectF& limits)
    : DragState(Qt::LeftButton), shape_(shape), handle_(handle),
      initialBounds_(shape.boundingRect()), limits_(limits),
      imageToShape_(shape.imageTransform().inverted()),
      anchorImage_(shape.handleCenter(SizeHandle{SizeHandle::oppositePosition(handle)})),
      initialRotation_(shape.rotationDegrees()) {}

void ResizeState::update(const QPointF& point) {
  const QPointF localPoint = imageToShape_.map(point);
  QRectF bounds = resizedBounds(initialBounds_, handle_, localPoint, limits_);
  const HandlePosition anchorHandle = SizeHandle::oppositePosition(handle_);
  const QPointF localAnchor = SizeHandle{anchorHandle}.center(bounds);
  const QPointF mappedAnchor = rotationTransform(bounds, initialRotation_).map(localAnchor);
  // Resizing changes the frame center, so compensate to keep the opposite handle fixed.
  bounds.translate(anchorImage_ - mappedAnchor);
  shape_.setBoundingRect(bounds);
}

std::optional<HandlePosition> ResizeState::activeHandle() const noexcept { return handle_; }

RotateState::RotateState(Shape& shape, HandlePosition handle, const QPointF& origin)
    : DragState(Qt::RightButton), shape_(shape), handle_(handle),
      initialRotation_(shape.rotationDegrees()),
      initialMouseAngle_(mouseAngle(shape.boundingRect().center(), origin)) {}

void RotateState::update(const QPointF& point) {
  const qreal currentMouseAngle = mouseAngle(shape_.boundingRect().center(), point);
  shape_.setRotationDegrees(initialRotation_ + currentMouseAngle - initialMouseAngle_);
}

std::optional<HandlePosition> RotateState::activeHandle() const noexcept { return handle_; }

} // namespace quickshot
