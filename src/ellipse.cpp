#include "quickshot/ellipse.hpp"

#include <QColor>
#include <QPainter>
#include <QPen>

namespace quickshot {
namespace {

QPen shapePen() {
  QPen pen{QColor{255, 190, 40}};
  pen.setCosmetic(true);
  pen.setWidth(2);
  return pen;
}

} // namespace

Ellipse::Ellipse(const QRectF& bounds) : bounds_(bounds.normalized()) {}

void Ellipse::draw(QPainter& painter) const {
  painter.save();
  painter.setPen(shapePen());
  painter.setBrush(Qt::NoBrush);
  painter.drawEllipse(bounds_);
  painter.restore();
}

QRectF Ellipse::boundingRect() const { return bounds_; }

void Ellipse::setBoundingRect(const QRectF& bounds) { bounds_ = bounds.normalized(); }

QPainterPath Ellipse::path() const {
  QPainterPath shapePath;
  shapePath.addEllipse(bounds_);
  return shapePath;
}

std::span<const SizeHandle> Ellipse::handles() const noexcept { return handles_; }

} // namespace quickshot
