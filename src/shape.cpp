#include "quickshot/shape.hpp"

namespace quickshot {

bool Shape::contains(const QPointF& point) const { return path().contains(point); }

void Shape::moveBy(const QPointF& offset) { setBoundingRect(boundingRect().translated(offset)); }

void Shape::transform(const QTransform& transformation) {
  setBoundingRect(transformation.mapRect(boundingRect()));
}

} // namespace quickshot
