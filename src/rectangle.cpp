#include "quickshot/rectangle.hpp"

#include <QColor>
#include <QPainter>
#include <QPen>

namespace quickshot {
namespace {

QPen shapePen() {
  QPen pen{QColor{0, 220, 120}};
  pen.setCosmetic(true);
  pen.setWidth(2);
  return pen;
}

} // namespace

Rectangle::Rectangle(const QRectF& bounds) : bounds_(bounds.normalized()) {}

void Rectangle::draw(QPainter& painter) const {
  painter.save();
  painter.setPen(shapePen());
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(bounds_);
  painter.restore();
}

QRectF Rectangle::boundingRect() const { return bounds_; }

void Rectangle::setBoundingRect(const QRectF& bounds) { bounds_ = bounds.normalized(); }

QPainterPath Rectangle::path() const {
  QPainterPath shapePath;
  shapePath.addRect(bounds_);
  return shapePath;
}

std::span<const SizeHandle> Rectangle::handles() const noexcept { return handles_; }

} // namespace quickshot
