#include "quickshot/drag_controller.hpp"

#include "quickshot/shapes/multi_point_shape.hpp"

#include <QCoreApplication>
#include <QtGlobal>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace quickshot {
namespace {

constexpr qreal minimumShapeSize = 1.0;

qreal mouseAngle(const QPointF& center, const QPointF& point) {
  return qRadiansToDegrees(std::atan2(point.y() - center.y(), point.x() - center.x()));
}

QRectF constrainedMove(const QRectF& bounds, const QPointF& offset, const QRectF& limits) {
  const qreal horizontalOffset =
      std::clamp(offset.x(), limits.left() - bounds.left(), limits.right() - bounds.right());
  const qreal verticalOffset =
      std::clamp(offset.y(), limits.top() - bounds.top(), limits.bottom() - bounds.bottom());
  return bounds.translated(horizontalOffset, verticalOffset);
}

Shape& targetShape(DragContext& context) {
  Q_ASSERT(context.shape != nullptr);
  return *context.shape;
}

const Shape& targetShape(const DragContext& context) {
  Q_ASSERT(context.shape != nullptr);
  return *context.shape;
}

const ShapeHandle& targetHandle(const DragContext& context) {
  Q_ASSERT(context.handle.has_value());
  return *context.handle;
}

MultiPointShape& targetMultiPointShape(DragContext& context) {
  auto* multiPointShape = dynamic_cast<MultiPointShape*>(context.shape);
  Q_ASSERT(multiPointShape != nullptr);
  return *multiPointShape;
}

const MultiPointShape& targetMultiPointShape(const DragContext& context) {
  const auto* multiPointShape = dynamic_cast<const MultiPointShape*>(context.shape);
  Q_ASSERT(multiPointShape != nullptr);
  return *multiPointShape;
}

QPointF boundedPoint(const QPointF& point, const QRectF& bounds) {
  return {std::clamp(point.x(), bounds.left(), bounds.right()),
          std::clamp(point.y(), bounds.top(), bounds.bottom())};
}

void restoreInitialGeometry(DragContext& context) {
  Q_ASSERT(context.initialGeometry != nullptr);
  if (context.initialGeometry != nullptr) {
    targetShape(context).restoreGeometry(*context.initialGeometry);
  }
}

} // namespace

void DragState::enter(DragContext& context) const { Q_UNUSED(context); }

DragProgress DragState::press(DragContext& context, Qt::MouseButton button,
                              const QPointF& point) const {
  Q_UNUSED(context);
  Q_UNUSED(button);
  Q_UNUSED(point);
  return DragProgress::Ignore;
}

DragProgress DragState::release(DragContext& context, Qt::MouseButton button,
                                const QPointF& point) const {
  Q_UNUSED(context);
  Q_UNUSED(point);
  return button == completionButton() ? DragProgress::Finish : DragProgress::Ignore;
}

DragResult DragState::finish(const DragContext& context) const noexcept {
  Q_UNUSED(context);
  return DragResult::KeepShape;
}

std::optional<ShapeHandle> DragState::activeHandle(const DragContext& context) const noexcept {
  Q_UNUSED(context);
  return std::nullopt;
}

bool DragState::createsShape() const noexcept { return false; }

QString DragState::undoText() const { return {}; }

const CreateState& CreateState::instance() noexcept {
  static const CreateState state;
  return state;
}

void CreateState::update(DragContext& context, const QPointF& point) const {
  targetShape(context).updateCreation(context.origin, context.imageBounds, point);
}

DragResult CreateState::finish(const DragContext& context) const noexcept {
  const QRectF bounds = targetShape(context).boundingRect();
  return bounds.width() < minimumShapeSize || bounds.height() < minimumShapeSize
             ? DragResult::RemoveShape
             : DragResult::KeepShape;
}

bool CreateState::createsShape() const noexcept { return true; }

Qt::MouseButton CreateState::completionButton() const noexcept { return Qt::LeftButton; }

const MultiPointCreateState& MultiPointCreateState::instance() noexcept {
  static const MultiPointCreateState state;
  return state;
}

void MultiPointCreateState::update(DragContext& context, const QPointF& point) const {
  targetMultiPointShape(context).setPreviewPoint(boundedPoint(point, context.imageBounds));
}

DragProgress MultiPointCreateState::press(DragContext& context, Qt::MouseButton button,
                                          const QPointF& point) const {
  if (button == Qt::LeftButton) {
    if (!context.imageBounds.contains(point)) {
      return DragProgress::Ignore;
    }
    targetMultiPointShape(context).appendPoint(point);
    return DragProgress::Continue;
  }
  if (button == Qt::RightButton) {
    // The right-click position is deliberately not a vertex; it only commits
    // the points previously added with the left button.
    targetMultiPointShape(context).finishCreation();
    return DragProgress::Finish;
  }
  return DragProgress::Ignore;
}

DragResult MultiPointCreateState::finish(const DragContext& context) const noexcept {
  return targetMultiPointShape(context).isCreationComplete() ? DragResult::KeepShape
                                                             : DragResult::RemoveShape;
}

bool MultiPointCreateState::createsShape() const noexcept { return true; }

Qt::MouseButton MultiPointCreateState::completionButton() const noexcept { return Qt::RightButton; }

const MoveState& MoveState::instance() noexcept {
  static const MoveState state;
  return state;
}

void MoveState::update(DragContext& context, const QPointF& point) const {
  restoreInitialGeometry(context);
  targetShape(context).setBoundingRect(
      constrainedMove(context.initialBounds, point - context.origin, context.imageBounds));
}

QString MoveState::undoText() const {
  return QCoreApplication::translate("MoveState", "Move Shape");
}

Qt::MouseButton MoveState::completionButton() const noexcept { return Qt::LeftButton; }

const ResizeState& ResizeState::instance() noexcept {
  static const ResizeState state;
  return state;
}

void ResizeState::update(DragContext& context, const QPointF& point) const {
  Q_ASSERT(context.initialGeometry != nullptr);
  if (context.initialGeometry == nullptr) {
    return;
  }
  targetShape(context).resize(*context.initialGeometry, targetHandle(context), point,
                              context.imageBounds);
}

std::optional<ShapeHandle> ResizeState::activeHandle(const DragContext& context) const noexcept {
  return context.handle;
}

QString ResizeState::undoText() const {
  return QCoreApplication::translate("ResizeState", "Resize Shape");
}

Qt::MouseButton ResizeState::completionButton() const noexcept { return Qt::LeftButton; }

const RotateState& RotateState::instance() noexcept {
  static const RotateState state;
  return state;
}

void RotateState::enter(DragContext& context) const {
  context.initialMouseAngle =
      mouseAngle(targetShape(context).boundingRect().center(), context.origin);
}

void RotateState::update(DragContext& context, const QPointF& point) const {
  restoreInitialGeometry(context);
  Shape& shape = targetShape(context);
  const qreal currentMouseAngle = mouseAngle(shape.boundingRect().center(), point);
  shape.setRotationDegrees(context.initialRotation + currentMouseAngle - context.initialMouseAngle);
}

std::optional<ShapeHandle> RotateState::activeHandle(const DragContext& context) const noexcept {
  return context.handle;
}

QString RotateState::undoText() const {
  return QCoreApplication::translate("RotateState", "Rotate Shape");
}

Qt::MouseButton RotateState::completionButton() const noexcept { return Qt::RightButton; }

void DragController::begin(const DragState& state, const DragStart& start) {
  Q_ASSERT(!isActive());
  if (isActive()) {
    return;
  }

  context_ = {
      .shape = &start.shape,
      .previousSelection = start.previousSelection,
      .handle = start.handle,
      .origin = start.origin,
      .imageBounds = start.imageBounds,
      .initialBounds = start.shape.boundingRect(),
      .initialRotation = start.shape.rotationDegrees(),
      .initialGeometry = state.createsShape() ? nullptr : start.shape.captureGeometry(),
      .initialMouseAngle = 0.0,
  };
  state_ = &state;
  state_->enter(context_);
}

void DragController::update(const QPointF& point) {
  if (state_ != nullptr) {
    state_->update(context_, point);
  }
}

DragProgress DragController::press(Qt::MouseButton button, const QPointF& point) {
  return state_ != nullptr ? state_->press(context_, button, point) : DragProgress::Ignore;
}

DragProgress DragController::release(Qt::MouseButton button, const QPointF& point) {
  return state_ != nullptr ? state_->release(context_, button, point) : DragProgress::Ignore;
}

std::optional<DragCompletion> DragController::finish() {
  if (state_ == nullptr) {
    return std::nullopt;
  }

  DragCompletion finishedDrag = completion(state_->finish(context_));
  reset();
  return finishedDrag;
}

std::optional<DragCompletion> DragController::cancel() {
  if (state_ == nullptr) {
    return std::nullopt;
  }

  const bool removesShape = state_->createsShape();
  if (!removesShape) {
    restoreInitialGeometry(context_);
  }
  DragCompletion cancelledDrag =
      completion(removesShape ? DragResult::RemoveShape : DragResult::KeepShape);
  reset();
  return cancelledDrag;
}

bool DragController::isActive() const noexcept { return state_ != nullptr; }

std::optional<ShapeHandle> DragController::activeHandle() const noexcept {
  return state_ != nullptr ? state_->activeHandle(context_) : std::nullopt;
}

Qt::MouseButton DragController::completionButton() const noexcept {
  return state_ != nullptr ? state_->completionButton() : Qt::NoButton;
}

DragCompletion DragController::completion(DragResult result) {
  Q_ASSERT(state_ != nullptr);
  Q_ASSERT(context_.shape != nullptr);
  return {
      .result = result,
      .createsShape = state_->createsShape(),
      .shape = context_.shape,
      .previousSelection = context_.previousSelection,
      .before = std::move(context_.initialGeometry),
      .after = state_->createsShape() ? nullptr : context_.shape->captureGeometry(),
      .undoText = state_->undoText(),
  };
}

void DragController::reset() noexcept {
  state_ = nullptr;
  context_ = {};
}

} // namespace quickshot
