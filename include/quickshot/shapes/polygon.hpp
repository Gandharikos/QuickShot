#pragma once

#include "quickshot/shapes/shape.hpp"

#include <optional>
#include <vector>

namespace quickshot {

class Polygon final : public Shape {
public:
  explicit Polygon(const QRectF& bounds);
  explicit Polygon(std::vector<QPointF> points);

  [[nodiscard]] std::unique_ptr<Shape> clone() const override;
  void draw(QPainter& painter) const override;
  [[nodiscard]] QRectF boundingRect() const override;
  void setBoundingRect(const QRectF& bounds) override;
  [[nodiscard]] QPainterPath path() const override;
  [[nodiscard]] std::span<const ShapeHandle> handles() const noexcept override;
  [[nodiscard]] QPointF handleCenter(const ShapeHandle& handle) const override;
  [[nodiscard]] std::unique_ptr<ShapeGeometry> captureGeometry() const override;
  void restoreGeometry(const ShapeGeometry& geometry) override;
  void resize(const ShapeGeometry& initialGeometry, const ShapeHandle& handle,
              const QPointF& imagePoint, const QRectF& imageBounds) override;
  [[nodiscard]] CreationKind creationKind() const noexcept override;
  [[nodiscard]] bool isCreationComplete() const noexcept override;

  void appendPoint(const QPointF& point);
  void setPreviewPoint(const QPointF& point);
  void finishCreation();
  [[nodiscard]] std::size_t pointCount() const noexcept;

private:
  class Geometry final : public ShapeGeometry {
  public:
    Geometry(std::vector<QPointF> points, qreal rotationDegrees, bool closed);

    [[nodiscard]] bool equals(const ShapeGeometry& other) const noexcept override;

    std::vector<QPointF> points;
    qreal rotationDegrees;
    bool closed;
  };

  void rebuildHandles();

  std::vector<QPointF> points_;
  std::vector<ShapeHandle> handles_;
  std::optional<QPointF> previewPoint_;
  bool closed_ = false;
};

} // namespace quickshot
