#pragma once

#include "quickshot/shapes/box_shape.hpp"

namespace quickshot {

class Ellipse : public BoxShape {
public:
  explicit Ellipse(const QRectF& bounds);

  [[nodiscard]] std::unique_ptr<Shape> clone() const override;
  void draw(QPainter& painter) const override;
  [[nodiscard]] QPainterPath path() const override;
};

} // namespace quickshot
