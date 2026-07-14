#pragma once

#include "quickshot/shape.hpp"

#include <array>

namespace quickshot {

class Rectangle final : public Shape {
public:
  explicit Rectangle(const QRectF& bounds);

  void draw(QPainter& painter) const override;
  [[nodiscard]] QRectF boundingRect() const override;
  void setBoundingRect(const QRectF& bounds) override;
  [[nodiscard]] QPainterPath path() const override;
  [[nodiscard]] std::span<const SizeHandle> handles() const noexcept override;

private:
  static constexpr std::array handles_ = {
      SizeHandle{HandlePosition::TopLeft},     SizeHandle{HandlePosition::Top},
      SizeHandle{HandlePosition::TopRight},    SizeHandle{HandlePosition::Right},
      SizeHandle{HandlePosition::BottomRight}, SizeHandle{HandlePosition::Bottom},
      SizeHandle{HandlePosition::BottomLeft},  SizeHandle{HandlePosition::Left},
  };

  QRectF bounds_;
};

} // namespace quickshot
