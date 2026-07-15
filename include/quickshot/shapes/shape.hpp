#pragma once

#include "quickshot/shapes/shape_handle.hpp"

#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QTransform>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <unordered_map>

class QPainter;

namespace quickshot {

enum class ShapeType : std::uint8_t { Rectangle, Ellipse, Count };

class Shape {
public:
  using Factory = std::function<std::unique_ptr<Shape>(const QRectF&)>;

  Shape() = default;
  virtual ~Shape() = default;

  Shape(const Shape&) = delete;
  Shape& operator=(const Shape&) = delete;
  Shape(Shape&&) = delete;
  Shape& operator=(Shape&&) = delete;

  [[nodiscard]] static std::unique_ptr<Shape> make(ShapeType type, const QRectF& bounds);
  [[nodiscard]] virtual std::unique_ptr<Shape> clone() const = 0;
  virtual void draw(QPainter& painter) const = 0;
  [[nodiscard]] virtual QRectF boundingRect() const = 0;
  virtual void setBoundingRect(const QRectF& bounds) = 0;
  [[nodiscard]] virtual QPainterPath path() const = 0;
  [[nodiscard]] virtual std::span<const ShapeHandle> handles() const noexcept = 0;

  [[nodiscard]] qreal rotationDegrees() const noexcept;
  void setRotationDegrees(qreal degrees) noexcept;
  [[nodiscard]] QTransform imageTransform() const;
  [[nodiscard]] QPointF mapToImage(const QPointF& point) const;
  [[nodiscard]] QPointF mapFromImage(const QPointF& point) const;
  [[nodiscard]] QPointF handleCenter(const ShapeHandle& handle) const;
  [[nodiscard]] bool contains(const QPointF& point) const;
  void moveBy(const QPointF& offset);
  void transform(const QTransform& transformation);

protected:
  [[nodiscard]] QPainterPath mapPathToImage(const QPainterPath& shapePath) const;

private:
  [[nodiscard]] static const std::unordered_map<ShapeType, Factory>& factories();

  qreal rotationDegrees_ = 0.0;
};

} // namespace quickshot
