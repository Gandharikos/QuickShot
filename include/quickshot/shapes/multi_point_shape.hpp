#pragma once

#include "quickshot/shapes/shape.hpp"

#include <QPolygonF>
#include <optional>
#include <vector>

namespace quickshot {

class MultiPointShape : public Shape {
public:
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
  [[nodiscard]] qsizetype pointCount() const noexcept;

protected:
  explicit MultiPointShape(const QRectF& bounds);
  explicit MultiPointShape(QPolygonF points);

  [[nodiscard]] const QPolygonF& points() const noexcept;
  [[nodiscard]] const std::optional<QPointF>& previewPoint() const noexcept;
  [[nodiscard]] bool isClosed() const noexcept;
  [[nodiscard]] virtual QPainterPath localPath() const = 0;

private:
  class Geometry final : public ShapeGeometry {
  public:
    Geometry(QPolygonF points, qreal rotationDegrees, bool closed, const QRectF& bounds);

    [[nodiscard]] bool equals(const ShapeGeometry& other) const noexcept override;

    QPolygonF points;
    qreal rotationDegrees;
    bool closed;
    QRectF bounds;
  };

  void rebuildHandles();

  QPolygonF points_;
  std::vector<ShapeHandle> handles_;
  std::optional<QPointF> previewPoint_;
  bool closed_ = false;
};

} // namespace quickshot
