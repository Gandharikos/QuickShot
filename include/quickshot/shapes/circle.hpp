#pragma once

#include "quickshot/shapes/ellipse.hpp"

namespace quickshot {

class Circle final : public Ellipse {
public:
  explicit Circle(const QRectF& bounds);

  [[nodiscard]] std::unique_ptr<Shape> clone() const override;
  void setBoundingRect(const QRectF& bounds) override;
  void updateCreation(const QPointF& origin, const QRectF& imageBounds,
                      const QPointF& imagePoint) override;

protected:
  [[nodiscard]] bool hasFixedAspectRatio() const noexcept override;
};

} // namespace quickshot
