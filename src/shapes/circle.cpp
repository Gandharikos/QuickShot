#include "quickshot/shapes/circle.hpp"

#include <algorithm>
#include <cmath>
#include <memory>

namespace quickshot {

Circle::Circle(const QRectF& bounds) : Ellipse(bounds) { setBoundingRect(bounds); }

void Circle::setBoundingRect(const QRectF& bounds) {
  const QRectF normalizedBounds = bounds.normalized();
  const qreal side = std::min(normalizedBounds.width(), normalizedBounds.height());
  const QPointF center = normalizedBounds.center();
  BoxShape::setBoundingRect({center.x() - side / 2.0, center.y() - side / 2.0, side, side});
}

std::unique_ptr<Shape> Circle::clone() const {
  auto circle = std::make_unique<Circle>(boundingRect());
  circle->setRotationDegrees(rotationDegrees());
  return circle;
}

void Circle::updateCreation(const QPointF& origin, const QRectF& imageBounds,
                            const QPointF& imagePoint) {
  const QPointF boundedPoint{std::clamp(imagePoint.x(), imageBounds.left(), imageBounds.right()),
                             std::clamp(imagePoint.y(), imageBounds.top(), imageBounds.bottom())};
  const qreal horizontalDirection = boundedPoint.x() < origin.x() ? -1.0 : 1.0;
  const qreal verticalDirection = boundedPoint.y() < origin.y() ? -1.0 : 1.0;
  const qreal requestedSide =
      std::max(std::abs(boundedPoint.x() - origin.x()), std::abs(boundedPoint.y() - origin.y()));
  const qreal horizontalMaximum = horizontalDirection < 0.0 ? origin.x() - imageBounds.left()
                                                            : imageBounds.right() - origin.x();
  const qreal verticalMaximum =
      verticalDirection < 0.0 ? origin.y() - imageBounds.top() : imageBounds.bottom() - origin.y();
  const qreal side = std::min({requestedSide, horizontalMaximum, verticalMaximum});
  BoxShape::setBoundingRect(
      QRectF{origin, origin + QPointF{horizontalDirection * side, verticalDirection * side}}
          .normalized());
}

bool Circle::hasFixedAspectRatio() const noexcept { return true; }

} // namespace quickshot
