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

  [[nodiscard]] bool contains(const QPointF& point) const;
  void moveBy(const QPointF& offset);
  void transform(const QTransform& transformation);
};

} // namespace quickshot
