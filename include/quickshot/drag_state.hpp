#pragma once

#include "quickshot/shapes/shape_handle.hpp"

#include <QPointF>
#include <QRectF>
#include <QTransform>
#include <Qt>
#include <cstdint>
#include <optional>

namespace quickshot {

class Shape;

enum class DragResult : std::uint8_t { KeepShape, RemoveShape };

class DragState {
public:
  explicit DragState(Qt::MouseButton completionButton) noexcept;
  virtual ~DragState() = default;

  DragState(const DragState&) = delete;
  DragState& operator=(const DragState&) = delete;
  DragState(DragState&&) = delete;
  DragState& operator=(DragState&&) = delete;

  virtual void update(const QPointF& point) = 0;
  [[nodiscard]] virtual DragResult finish() const noexcept;
  [[nodiscard]] virtual std::optional<HandlePosition> activeHandle() const noexcept;
  [[nodiscard]] Qt::MouseButton completionButton() const noexcept;

private:
  Qt::MouseButton completionButton_;
};

class CreateState final : public DragState {
public:
  CreateState(Shape& shape, const QPointF& origin, const QRectF& limits) noexcept;

  void update(const QPointF& point) override;
  [[nodiscard]] DragResult finish() const noexcept override;

private:
  Shape& shape_;
  QPointF origin_;
  QRectF limits_;
};

class MoveState final : public DragState {
public:
  MoveState(Shape& shape, const QPointF& origin, const QRectF& limits);

  void update(const QPointF& point) override;

private:
  Shape& shape_;
  QPointF origin_;
  QRectF initialBounds_;
  QRectF limits_;
};

class ResizeState final : public DragState {
public:
  ResizeState(Shape& shape, HandlePosition handle, const QRectF& limits);

  void update(const QPointF& point) override;
  [[nodiscard]] std::optional<HandlePosition> activeHandle() const noexcept override;

private:
  Shape& shape_;
  HandlePosition handle_;
  QRectF initialBounds_;
  QRectF limits_;
  QTransform imageToShape_;
  QPointF anchorImage_;
  qreal initialRotation_;
};

class RotateState final : public DragState {
public:
  RotateState(Shape& shape, HandlePosition handle, const QPointF& origin);

  void update(const QPointF& point) override;
  [[nodiscard]] std::optional<HandlePosition> activeHandle() const noexcept override;

private:
  Shape& shape_;
  HandlePosition handle_;
  qreal initialRotation_;
  qreal initialMouseAngle_;
};

} // namespace quickshot
