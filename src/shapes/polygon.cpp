#include "quickshot/shapes/polygon.hpp"

#include <memory>
#include <utility>

namespace quickshot {

Polygon::Polygon(const QRectF& bounds) : MultiPointShape(bounds) {}

Polygon::Polygon(QPolygonF points) : MultiPointShape(std::move(points)) {}

std::unique_ptr<Shape> Polygon::clone() const {
  auto polygon = std::make_unique<Polygon>(points());
  polygon->setRotationDegrees(rotationDegrees());
  return polygon;
}

QPainterPath Polygon::localPath() const {
  QPainterPath shapePath;
  if (points().empty()) {
    return shapePath;
  }

  shapePath.addPolygon(points());
  if (isClosed()) {
    shapePath.closeSubpath();
  } else if (previewPoint().has_value()) {
    shapePath.lineTo(*previewPoint());
  }
  return shapePath;
}

} // namespace quickshot
