#include "quickshot/shapes/rectangular_shape.hpp"

#include <QtGlobal>
#include <algorithm>

namespace quickshot {
namespace {

constexpr qreal minimumShapeSize = 1.0;

HandlePosition handlePosition(const ShapeHandle& handle) {
  Q_ASSERT(handle.id() <= static_cast<ShapeHandle::Id>(HandlePosition::Left));
  return static_cast<HandlePosition>(handle.id());
}

HandlePosition oppositePosition(HandlePosition position) {
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

QPointF rectangularHandleCenter(const QRectF& bounds, HandlePosition position) {
  switch (position) {
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

QTransform rotationTransform(const QRectF& bounds, qreal degrees) {
  const QPointF center = bounds.center();
  QTransform transformation;
  transformation.translate(center.x(), center.y());
  transformation.rotate(degrees);
  transformation.translate(-center.x(), -center.y());
  return transformation;
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

RectangularShape::Geometry::Geometry(const QRectF& bounds, qreal rotationDegrees)
    : bounds(bounds), rotationDegrees(rotationDegrees) {}

bool RectangularShape::Geometry::equals(const ShapeGeometry& other) const noexcept {
  const auto* geometry = dynamic_cast<const Geometry*>(&other);
  return geometry != nullptr && bounds == geometry->bounds &&
         qFuzzyCompare(rotationDegrees + 1.0, geometry->rotationDegrees + 1.0);
}

RectangularShape::RectangularShape(const QRectF& bounds) : bounds_(bounds.normalized()) {}

QRectF RectangularShape::boundingRect() const { return bounds_; }

void RectangularShape::setBoundingRect(const QRectF& bounds) { bounds_ = bounds.normalized(); }

std::span<const ShapeHandle> RectangularShape::handles() const noexcept { return handles_; }

QPointF RectangularShape::handleCenter(const ShapeHandle& handle) const {
  return mapToImage(rectangularHandleCenter(bounds_, handlePosition(handle)));
}

std::unique_ptr<ShapeGeometry> RectangularShape::captureGeometry() const {
  return std::make_unique<Geometry>(bounds_, rotationDegrees());
}

void RectangularShape::restoreGeometry(const ShapeGeometry& geometry) {
  const auto* rectangularGeometry = dynamic_cast<const Geometry*>(&geometry);
  Q_ASSERT(rectangularGeometry != nullptr);
  if (rectangularGeometry == nullptr) {
    return;
  }

  bounds_ = rectangularGeometry->bounds;
  setRotationDegrees(rectangularGeometry->rotationDegrees);
}

void RectangularShape::resize(const ShapeGeometry& initialGeometry, const ShapeHandle& handle,
                              const QPointF& imagePoint, const QRectF& imageBounds) {
  const auto* geometry = dynamic_cast<const Geometry*>(&initialGeometry);
  Q_ASSERT(geometry != nullptr);
  if (geometry == nullptr) {
    return;
  }

  const HandlePosition position = handlePosition(handle);
  const QTransform initialTransform =
      rotationTransform(geometry->bounds, geometry->rotationDegrees);
  const QPointF localPoint = initialTransform.inverted().map(imagePoint);
  QRectF bounds = resizedBounds(geometry->bounds, position, localPoint, imageBounds);
  const QPointF anchorImage =
      initialTransform.map(rectangularHandleCenter(geometry->bounds, oppositePosition(position)));
  const QPointF localAnchor = rectangularHandleCenter(bounds, oppositePosition(position));
  const QPointF mappedAnchor =
      rotationTransform(bounds, geometry->rotationDegrees).map(localAnchor);
  bounds.translate(anchorImage - mappedAnchor);

  bounds_ = bounds;
  setRotationDegrees(geometry->rotationDegrees);
}

} // namespace quickshot
