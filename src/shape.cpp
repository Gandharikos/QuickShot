#include "quickshot/shape.hpp"

#include "quickshot/ellipse.hpp"
#include "quickshot/rectangle.hpp"

#include <QLineF>
#include <QtMath>
#include <cmath>
#include <memory>
#include <stdexcept>

namespace quickshot {

const std::unordered_map<ShapeType, Shape::Factory>& Shape::factories() {
  static const std::unordered_map<ShapeType, Factory> registeredFactories{
      {ShapeType::Rectangle,
       [](const QRectF& bounds) -> std::unique_ptr<Shape> {
         return std::make_unique<Rectangle>(bounds);
       }},
      {ShapeType::Ellipse,
       [](const QRectF& bounds) -> std::unique_ptr<Shape> {
         return std::make_unique<Ellipse>(bounds);
       }},
  };
  return registeredFactories;
}

std::unique_ptr<Shape> Shape::make(ShapeType type, const QRectF& bounds) {
  const auto& registeredFactories = factories();
  const auto factory = registeredFactories.find(type);
  if (factory == registeredFactories.end()) {
    throw std::invalid_argument{"Unregistered ShapeType"};
  }

  return factory->second(bounds);
}

qreal Shape::rotationDegrees() const noexcept { return rotationDegrees_; }

void Shape::setRotationDegrees(qreal degrees) noexcept {
  rotationDegrees_ = std::fmod(degrees, 360.0);
  if (rotationDegrees_ < 0.0) {
    rotationDegrees_ += 360.0;
  }
}

QTransform Shape::imageTransform() const {
  const QPointF center = boundingRect().center();
  QTransform transformation;
  transformation.translate(center.x(), center.y());
  transformation.rotate(rotationDegrees_);
  transformation.translate(-center.x(), -center.y());
  return transformation;
}

QPointF Shape::mapToImage(const QPointF& point) const { return imageTransform().map(point); }

QPointF Shape::mapFromImage(const QPointF& point) const {
  return imageTransform().inverted().map(point);
}

QPointF Shape::handleCenter(const SizeHandle& handle) const {
  return mapToImage(handle.center(boundingRect()));
}

bool Shape::contains(const QPointF& point) const { return path().contains(point); }

void Shape::moveBy(const QPointF& offset) { setBoundingRect(boundingRect().translated(offset)); }

void Shape::transform(const QTransform& transformation) {
  const QRectF bounds = boundingRect();
  const QPointF center = bounds.center();
  const QPointF transformedCenter = transformation.map(center);
  // Transform the oriented axes separately so image rotation preserves both size and angle.
  const QPointF transformedHorizontal =
      transformation.map(mapToImage(center + QPointF{bounds.width() / 2.0, 0.0}));
  const QPointF transformedVertical =
      transformation.map(mapToImage(center + QPointF{0.0, bounds.height() / 2.0}));
  const qreal transformedWidth = 2.0 * QLineF{transformedCenter, transformedHorizontal}.length();
  const qreal transformedHeight = 2.0 * QLineF{transformedCenter, transformedVertical}.length();
  const QPointF horizontalDirection = transformedHorizontal - transformedCenter;
  const qreal transformedRotation =
      qRadiansToDegrees(std::atan2(horizontalDirection.y(), horizontalDirection.x()));

  setBoundingRect(QRectF{transformedCenter.x() - transformedWidth / 2.0,
                         transformedCenter.y() - transformedHeight / 2.0, transformedWidth,
                         transformedHeight});
  setRotationDegrees(transformedRotation);
}

QPainterPath Shape::mapPathToImage(const QPainterPath& shapePath) const {
  return imageTransform().map(shapePath);
}

} // namespace quickshot
