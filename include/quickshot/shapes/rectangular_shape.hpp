#pragma once

#include "quickshot/shapes/shape.hpp"

#include <array>

namespace quickshot {

class RectangularShape : public Shape {
public:
  [[nodiscard]] QRectF boundingRect() const override;
  void setBoundingRect(const QRectF& bounds) override;
  [[nodiscard]] std::span<const ShapeHandle> handles() const noexcept override;
  [[nodiscard]] QPointF handleCenter(const ShapeHandle& handle) const override;
  [[nodiscard]] std::unique_ptr<ShapeGeometry> captureGeometry() const override;
  void restoreGeometry(const ShapeGeometry& geometry) override;
  void resize(const ShapeGeometry& initialGeometry, const ShapeHandle& handle,
              const QPointF& imagePoint, const QRectF& imageBounds) override;

protected:
  explicit RectangularShape(const QRectF& bounds);
  [[nodiscard]] virtual bool hasFixedAspectRatio() const noexcept;

private:
  class Geometry final : public ShapeGeometry {
  public:
    Geometry(const QRectF& bounds, qreal rotationDegrees);

    [[nodiscard]] bool equals(const ShapeGeometry& other) const noexcept override;

    QRectF bounds;
    qreal rotationDegrees;
  };

  static constexpr std::array handles_ = {
      ShapeHandle{HandlePosition::TopLeft},     ShapeHandle{HandlePosition::Top},
      ShapeHandle{HandlePosition::TopRight},    ShapeHandle{HandlePosition::Right},
      ShapeHandle{HandlePosition::BottomRight}, ShapeHandle{HandlePosition::Bottom},
      ShapeHandle{HandlePosition::BottomLeft},  ShapeHandle{HandlePosition::Left},
  };

  QRectF bounds_;
};

} // namespace quickshot
