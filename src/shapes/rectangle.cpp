#include "quickshot/shapes/rectangle.hpp"

#include <QPainter>
#include <memory>

namespace quickshot {

Rectangle::Rectangle(const QRectF& bounds) : bounds_(bounds.normalized()) {}

std::unique_ptr<Shape> Rectangle::clone() const {
  auto rectangle = std::make_unique<Rectangle>(bounds_);
  rectangle->setRotationDegrees(rotationDegrees());
  return rectangle;
}

void Rectangle::draw(QPainter& painter) const { painter.drawPath(path()); }

QRectF Rectangle::boundingRect() const { return bounds_; }

void Rectangle::setBoundingRect(const QRectF& bounds) { bounds_ = bounds.normalized(); }

QPainterPath Rectangle::path() const {
  QPainterPath shapePath;
  shapePath.addRect(bounds_);
  return mapPathToImage(shapePath);
}

std::span<const ShapeHandle> Rectangle::handles() const noexcept { return handles_; }

} // namespace quickshot
