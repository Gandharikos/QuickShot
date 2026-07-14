#include "quickshot/rectangle.hpp"

#include <QPainter>
#include <memory>

namespace quickshot {

Rectangle::Rectangle(const QRectF& bounds) : bounds_(bounds.normalized()) {}

std::unique_ptr<Shape> Rectangle::clone() const { return std::make_unique<Rectangle>(bounds_); }

void Rectangle::draw(QPainter& painter) const { painter.drawRect(bounds_); }

QRectF Rectangle::boundingRect() const { return bounds_; }

void Rectangle::setBoundingRect(const QRectF& bounds) { bounds_ = bounds.normalized(); }

QPainterPath Rectangle::path() const {
  QPainterPath shapePath;
  shapePath.addRect(bounds_);
  return shapePath;
}

std::span<const SizeHandle> Rectangle::handles() const noexcept { return handles_; }

} // namespace quickshot
