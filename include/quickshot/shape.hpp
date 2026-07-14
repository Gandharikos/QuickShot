#pragma once

#include "quickshot/size_handle.hpp"

#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QTransform>
#include <memory>
#include <span>

class QPainter;

namespace quickshot {

class Shape {
public:
  Shape() = default;
  virtual ~Shape() = default;

  Shape(const Shape&) = delete;
  Shape& operator=(const Shape&) = delete;
  Shape(Shape&&) = delete;
  Shape& operator=(Shape&&) = delete;

  [[nodiscard]] virtual std::unique_ptr<Shape> clone() const = 0;
  virtual void draw(QPainter& painter) const = 0;
  [[nodiscard]] virtual QRectF boundingRect() const = 0;
  virtual void setBoundingRect(const QRectF& bounds) = 0;
  [[nodiscard]] virtual QPainterPath path() const = 0;
  [[nodiscard]] virtual std::span<const SizeHandle> handles() const noexcept = 0;

  [[nodiscard]] qreal rotationDegrees() const noexcept;
  void setRotationDegrees(qreal degrees) noexcept;
  [[nodiscard]] QTransform imageTransform() const;
  [[nodiscard]] QPointF mapToImage(const QPointF& point) const;
  [[nodiscard]] QPointF mapFromImage(const QPointF& point) const;
  [[nodiscard]] QPointF handleCenter(const SizeHandle& handle) const;
  [[nodiscard]] bool contains(const QPointF& point) const;
  void moveBy(const QPointF& offset);
  void transform(const QTransform& transformation);

protected:
  [[nodiscard]] QPainterPath mapPathToImage(const QPainterPath& shapePath) const;

private:
  qreal rotationDegrees_ = 0.0;
};

} // namespace quickshot
