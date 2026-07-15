#pragma once

#include "quickshot/shapes/shape.hpp"

#include <QPointF>
#include <QRectF>
#include <QString>
#include <Qt>
#include <cstdint>
#include <memory>
#include <optional>

namespace quickshot {

enum class DragResult : std::uint8_t {
  KeepShape,
  // CreateState updates a provisional shape already inserted for live preview;
  // remove it when the completed gesture is too small to produce a valid shape.
  RemoveShape,
};

enum class DragProgress : std::uint8_t { Ignore, Continue, Finish };

struct DragStart {
  Shape& shape;
  QPointF origin;
  QRectF imageBounds;
  std::optional<ShapeHandle> handle;
  Shape* previousSelection = nullptr;
};

struct DragCompletion {
  DragResult result;
  bool createsShape;
  Shape* shape;
  Shape* previousSelection;
  std::unique_ptr<ShapeGeometry> before;
  std::unique_ptr<ShapeGeometry> after;
  QString undoText;
};

struct DragContext {
  Shape* shape = nullptr;
  Shape* previousSelection = nullptr;
  std::optional<ShapeHandle> handle;
  QPointF origin;
  QRectF imageBounds;
  QRectF initialBounds;
  qreal initialRotation = 0.0;
  std::unique_ptr<ShapeGeometry> initialGeometry;
  qreal initialMouseAngle = 0.0;
};

class DragState {
public:
  virtual ~DragState() = default;

  DragState(const DragState&) = delete;
  DragState& operator=(const DragState&) = delete;
  DragState(DragState&&) = delete;
  DragState& operator=(DragState&&) = delete;

  virtual void enter(DragContext& context) const;
  virtual void update(DragContext& context, const QPointF& point) const = 0;
  [[nodiscard]] virtual DragProgress press(DragContext& context, Qt::MouseButton button,
                                           const QPointF& point) const;
  [[nodiscard]] virtual DragProgress release(DragContext& context, Qt::MouseButton button,
                                             const QPointF& point) const;
  [[nodiscard]] virtual DragResult finish(const DragContext& context) const noexcept;
  [[nodiscard]] virtual std::optional<ShapeHandle>
  activeHandle(const DragContext& context) const noexcept;
  [[nodiscard]] virtual bool createsShape() const noexcept;
  [[nodiscard]] virtual QString undoText() const;
  [[nodiscard]] virtual Qt::MouseButton completionButton() const noexcept = 0;

protected:
  DragState() = default;
};

class CreateState final : public DragState {
public:
  [[nodiscard]] static const CreateState& instance() noexcept;

  void update(DragContext& context, const QPointF& point) const override;
  [[nodiscard]] DragResult finish(const DragContext& context) const noexcept override;
  [[nodiscard]] bool createsShape() const noexcept override;
  [[nodiscard]] Qt::MouseButton completionButton() const noexcept override;

private:
  CreateState() = default;
};

class MoveState final : public DragState {
public:
  [[nodiscard]] static const MoveState& instance() noexcept;

  void update(DragContext& context, const QPointF& point) const override;
  [[nodiscard]] QString undoText() const override;
  [[nodiscard]] Qt::MouseButton completionButton() const noexcept override;

private:
  MoveState() = default;
};

class PolygonCreateState final : public DragState {
public:
  [[nodiscard]] static const PolygonCreateState& instance() noexcept;

  void update(DragContext& context, const QPointF& point) const override;
  [[nodiscard]] DragProgress press(DragContext& context, Qt::MouseButton button,
                                   const QPointF& point) const override;
  [[nodiscard]] DragResult finish(const DragContext& context) const noexcept override;
  [[nodiscard]] bool createsShape() const noexcept override;
  [[nodiscard]] Qt::MouseButton completionButton() const noexcept override;

private:
  PolygonCreateState() = default;
};

class ResizeState final : public DragState {
public:
  [[nodiscard]] static const ResizeState& instance() noexcept;

  void update(DragContext& context, const QPointF& point) const override;
  [[nodiscard]] std::optional<ShapeHandle>
  activeHandle(const DragContext& context) const noexcept override;
  [[nodiscard]] QString undoText() const override;
  [[nodiscard]] Qt::MouseButton completionButton() const noexcept override;

private:
  ResizeState() = default;
};

class RotateState final : public DragState {
public:
  [[nodiscard]] static const RotateState& instance() noexcept;

  void enter(DragContext& context) const override;
  void update(DragContext& context, const QPointF& point) const override;
  [[nodiscard]] std::optional<ShapeHandle>
  activeHandle(const DragContext& context) const noexcept override;
  [[nodiscard]] QString undoText() const override;
  [[nodiscard]] Qt::MouseButton completionButton() const noexcept override;

private:
  RotateState() = default;
};

class DragController final {
public:
  DragController() = default;

  DragController(const DragController&) = delete;
  DragController& operator=(const DragController&) = delete;
  DragController(DragController&&) = delete;
  DragController& operator=(DragController&&) = delete;

  void begin(const DragState& state, const DragStart& start);
  void update(const QPointF& point);
  [[nodiscard]] DragProgress press(Qt::MouseButton button, const QPointF& point);
  [[nodiscard]] DragProgress release(Qt::MouseButton button, const QPointF& point);
  [[nodiscard]] std::optional<DragCompletion> finish();
  [[nodiscard]] std::optional<DragCompletion> cancel();
  [[nodiscard]] bool isActive() const noexcept;
  [[nodiscard]] std::optional<ShapeHandle> activeHandle() const noexcept;
  [[nodiscard]] Qt::MouseButton completionButton() const noexcept;

private:
  [[nodiscard]] DragCompletion completion(DragResult result);
  void reset() noexcept;

  const DragState* state_ = nullptr;
  DragContext context_;
};

} // namespace quickshot
