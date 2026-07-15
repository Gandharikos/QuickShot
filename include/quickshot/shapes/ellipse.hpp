#pragma once

#include "quickshot/shapes/shape.hpp"

#include <array>

namespace quickshot {

class Ellipse final : public Shape {
public:
  explicit Ellipse(const QRectF& bounds);

  [[nodiscard]] std::unique_ptr<Shape> clone() const override;
  void draw(QPainter& painter) const override;
  [[nodiscard]] QRectF boundingRect() const override;
  void setBoundingRect(const QRectF& bounds) override;
  [[nodiscard]] QPainterPath path() const override;
  [[nodiscard]] std::span<const ShapeHandle> handles() const noexcept override;

private:
  static constexpr std::array handles_ = {
      ShapeHandle{HandlePosition::TopLeft},     ShapeHandle{HandlePosition::Top},
      ShapeHandle{HandlePosition::TopRight},    ShapeHandle{HandlePosition::Right},
      ShapeHandle{HandlePosition::BottomRight}, ShapeHandle{HandlePosition::Bottom},
      ShapeHandle{HandlePosition::BottomLeft},  ShapeHandle{HandlePosition::Left},
  };

  QRectF bounds_;
};

} // namespace quickshot
