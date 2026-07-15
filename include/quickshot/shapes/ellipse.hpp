#pragma once

#include "quickshot/shapes/rectangular_shape.hpp"

namespace quickshot {

class Ellipse final : public RectangularShape {
public:
  explicit Ellipse(const QRectF& bounds);

  [[nodiscard]] std::unique_ptr<Shape> clone() const override;
  void draw(QPainter& painter) const override;
  [[nodiscard]] QPainterPath path() const override;
};

} // namespace quickshot
