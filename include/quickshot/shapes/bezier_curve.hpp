#pragma once

#include "quickshot/shapes/multi_point_shape.hpp"

namespace quickshot {

class BezierCurve final : public MultiPointShape {
public:
  explicit BezierCurve(const QRectF& bounds);
  explicit BezierCurve(QPolygonF points);

  [[nodiscard]] std::unique_ptr<Shape> clone() const override;

private:
  [[nodiscard]] QPainterPath localPath() const override;
};

} // namespace quickshot
