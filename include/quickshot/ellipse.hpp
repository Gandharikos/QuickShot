#pragma once

#include "quickshot/shape.hpp"

#include <array>

namespace quickshot {

class Ellipse final : public Shape {
public:
  explicit Ellipse(const QRectF& bounds);

  void draw(QPainter& painter) const override;
  [[nodiscard]] QRectF boundingRect() const override;
  void setBoundingRect(const QRectF& bounds) override;
  [[nodiscard]] QPainterPath path() const override;
  [[nodiscard]] std::span<const SizeHandle> handles() const noexcept override;

private:
  static constexpr std::array handles_ = {
      SizeHandle{HandlePosition::Top},
      SizeHandle{HandlePosition::Right},
      SizeHandle{HandlePosition::Bottom},
      SizeHandle{HandlePosition::Left},
  };

  QRectF bounds_;
};

} // namespace quickshot
