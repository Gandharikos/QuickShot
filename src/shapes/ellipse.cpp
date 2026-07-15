#include "quickshot/shapes/ellipse.hpp"

#include <QPainter>
#include <memory>

namespace quickshot {

Ellipse::Ellipse(const QRectF& bounds) : BoxShape(bounds) {}

std::unique_ptr<Shape> Ellipse::clone() const {
  auto ellipse = std::make_unique<Ellipse>(boundingRect());
  ellipse->setRotationDegrees(rotationDegrees());
  return ellipse;
}

void Ellipse::draw(QPainter& painter) const { painter.drawPath(path()); }

QPainterPath Ellipse::path() const {
  QPainterPath shapePath;
  shapePath.addEllipse(boundingRect());
  return mapPathToImage(shapePath);
}

} // namespace quickshot
