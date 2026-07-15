#include "quickshot/shapes/rectangle.hpp"

#include <QPainter>
#include <memory>

namespace quickshot {

Rectangle::Rectangle(const QRectF& bounds) : BoxShape(bounds) {}

std::unique_ptr<Shape> Rectangle::clone() const {
  auto rectangle = std::make_unique<Rectangle>(boundingRect());
  rectangle->setRotationDegrees(rotationDegrees());
  return rectangle;
}

void Rectangle::draw(QPainter& painter) const { painter.drawPath(path()); }

QPainterPath Rectangle::path() const {
  QPainterPath shapePath;
  shapePath.addRect(boundingRect());
  return mapPathToImage(shapePath);
}

} // namespace quickshot
