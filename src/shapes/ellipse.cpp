#include "quickshot/shapes/ellipse.hpp"

#include <QPainter>
#include <memory>

namespace quickshot {

Ellipse::Ellipse(const QRectF& bounds) : bounds_(bounds.normalized()) {}

std::unique_ptr<Shape> Ellipse::clone() const {
  auto ellipse = std::make_unique<Ellipse>(bounds_);
  ellipse->setRotationDegrees(rotationDegrees());
  return ellipse;
}

void Ellipse::draw(QPainter& painter) const { painter.drawPath(path()); }

QRectF Ellipse::boundingRect() const { return bounds_; }

void Ellipse::setBoundingRect(const QRectF& bounds) { bounds_ = bounds.normalized(); }

QPainterPath Ellipse::path() const {
  QPainterPath shapePath;
  shapePath.addEllipse(bounds_);
  return mapPathToImage(shapePath);
}

std::span<const SizeHandle> Ellipse::handles() const noexcept { return handles_; }

} // namespace quickshot
