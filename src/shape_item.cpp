#include "quickshot/shape_item.hpp"

#include "quickshot/image_scene.hpp"
#include "quickshot/shapes/multi_point_shape.hpp"

#include <QBrush>
#include <QColor>
#include <QCursor>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QStyleOptionGraphicsItem>
#include <QSvgRenderer>
#include <QtMath>
#include <cmath>
#include <utility>

namespace quickshot {
namespace {

constexpr qreal handleSize = 8.0;

QCursor rotationCursor() {
  static const QCursor cursor = []() {
    QPixmap cursorPixmap{32, 32};
    cursorPixmap.fill(Qt::transparent);
    QSvgRenderer renderer{QStringLiteral(":/quickshot/icons/rotate-cursor.svg")};
    QPainter painter{&cursorPixmap};
    renderer.render(&painter);
    return QCursor{cursorPixmap, 16, 16};
  }();
  return cursor;
}

qreal mouseAngle(const QPointF& center, const QPointF& point) {
  return qRadiansToDegrees(std::atan2(point.y() - center.y(), point.x() - center.x()));
}

} // namespace

class ShapeHandleItem final : public QGraphicsItem {
public:
  ShapeHandleItem(ShapeItem& shapeItem, ShapeHandle handle)
      : QGraphicsItem(&shapeItem), shapeItem_(shapeItem), handle_(handle) {
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setAcceptHoverEvents(true);
    setFlag(ItemIgnoresTransformations);
    setZValue(1.0);
    setCursor(handle_.cursorShape());
  }

  [[nodiscard]] QRectF boundingRect() const override {
    return {-handleSize / 2.0, -handleSize / 2.0, handleSize, handleSize};
  }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override {
    Q_UNUSED(option);
    Q_UNUSED(widget);
    const QColor color =
        shapeItem_.model().isCreationComplete() ? QColor{Qt::white} : QColor{0, 200, 83};
    painter->setPen(QPen{color});
    painter->setBrush(QBrush{color});
    painter->drawRect(boundingRect());
  }

protected:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override {
    setCursor(handle_.cursorShape());
    QGraphicsItem::hoverEnterEvent(event);
  }

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
    shapeItem_.beginHandleInteraction(handle_, event->button(), event->scenePos());
    if (event->button() == Qt::RightButton) {
      setCursor(rotationCursor());
    }
    event->accept();
  }

  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override {
    shapeItem_.updateHandleInteraction(event->scenePos());
    event->accept();
  }

  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override {
    shapeItem_.finishHandleInteraction();
    setCursor(handle_.cursorShape());
    event->accept();
  }

  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override { event->accept(); }

private:
  ShapeItem& shapeItem_;
  ShapeHandle handle_;
};

ShapeItem::ShapeItem(std::unique_ptr<Shape> shape, QGraphicsItem* parent)
    : QGraphicsObject(parent), shape_(std::move(shape)) {
  Q_ASSERT(shape_ != nullptr);
  setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
  setAcceptHoverEvents(true);
  setCursor(Qt::CrossCursor);
  rebuildHandles();
}

ShapeItem::~ShapeItem() = default;

QRectF ShapeItem::boundingRect() const { return shape().boundingRect().adjusted(-2, -2, 2, 2); }

QPainterPath ShapeItem::shape() const { return shape_->path(); }

void ShapeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) {
  Q_UNUSED(option);
  Q_UNUSED(widget);
  QPen pen{isSelected() ? QColor{Qt::red} : QColor{Qt::white}};
  pen.setCosmetic(true);
  pen.setWidth(2);
  painter->setPen(pen);
  painter->setBrush(Qt::NoBrush);
  painter->setRenderHint(QPainter::Antialiasing);
  shape_->draw(*painter);
}

const Shape& ShapeItem::model() const noexcept { return *shape_; }

Shape& ShapeItem::model() noexcept { return *shape_; }

std::unique_ptr<ShapeItem> ShapeItem::clone() const {
  auto item = std::make_unique<ShapeItem>(shape_->clone());
  item->setPos(pos());
  item->setZValue(zValue());
  return item;
}

QPainterPath ShapeItem::imagePath() const {
  // The parent content item may be rotated in the scene, but ROI geometry remains in the original
  // image coordinate system until export applies the document's display transform once.
  return mapToParent(shape());
}

ShapeItemGeometry ShapeItem::captureGeometry() const {
  return {.position = pos(), .shape = shape_->captureGeometry()};
}

void ShapeItem::restoreGeometry(const ShapeItemGeometry& geometry) {
  Q_ASSERT(geometry.shape != nullptr);
  if (geometry.shape == nullptr) {
    return;
  }
  prepareGeometryChange();
  shape_->restoreGeometry(*geometry.shape);
  setPos(geometry.position);
  updateHandles();
  update();
}

void ShapeItem::updateCreation(const QPointF& origin, const QRectF& imageBounds,
                               const QPointF& imagePoint) {
  prepareGeometryChange();
  shape_->updateCreation(origin, imageBounds, imagePoint);
  rebuildHandles();
  update();
}

void ShapeItem::appendCreationPoint(const QPointF& point) {
  auto* multiPointShape = dynamic_cast<MultiPointShape*>(shape_.get());
  Q_ASSERT(multiPointShape != nullptr);
  if (multiPointShape == nullptr) {
    return;
  }
  prepareGeometryChange();
  multiPointShape->appendPoint(point);
  rebuildHandles();
  update();
}

void ShapeItem::setCreationPreview(const QPointF& point) {
  auto* multiPointShape = dynamic_cast<MultiPointShape*>(shape_.get());
  Q_ASSERT(multiPointShape != nullptr);
  if (multiPointShape == nullptr) {
    return;
  }
  prepareGeometryChange();
  multiPointShape->setPreviewPoint(point);
  update();
}

void ShapeItem::finishCreation() {
  auto* multiPointShape = dynamic_cast<MultiPointShape*>(shape_.get());
  Q_ASSERT(multiPointShape != nullptr);
  if (multiPointShape == nullptr) {
    return;
  }
  prepareGeometryChange();
  multiPointShape->finishCreation();
  rebuildHandles();
  update();
}

QVariant ShapeItem::itemChange(GraphicsItemChange change, const QVariant& value) {
  if (change == ItemSelectedHasChanged) {
    updateHandleVisibility();
    update();
  } else if (change == ItemPositionChange && scene() != nullptr) {
    const QPointF proposedPosition = value.toPointF();
    const QPointF delta = proposedPosition - pos();
    const QRectF proposedBounds = imagePath().boundingRect().translated(delta);
    const ImageScene* currentScene = imageScene();
    Q_ASSERT(currentScene != nullptr);
    if (currentScene == nullptr) {
      return value;
    }
    const QRectF limits = currentScene->imageBounds();
    QPointF constrained = proposedPosition;
    if (proposedBounds.left() < limits.left()) {
      constrained.rx() += limits.left() - proposedBounds.left();
    }
    if (proposedBounds.right() > limits.right()) {
      constrained.rx() -= proposedBounds.right() - limits.right();
    }
    if (proposedBounds.top() < limits.top()) {
      constrained.ry() += limits.top() - proposedBounds.top();
    }
    if (proposedBounds.bottom() > limits.bottom()) {
      constrained.ry() -= proposedBounds.bottom() - limits.bottom();
    }
    return constrained;
  }
  return QGraphicsObject::itemChange(change, value);
}

void ShapeItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    interactionStart_ = std::make_unique<ShapeItemGeometry>(captureGeometry());
  }
  QGraphicsObject::mousePressEvent(event);
}

void ShapeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
  QGraphicsObject::mouseReleaseEvent(event);
  if (event->button() == Qt::LeftButton && interactionStart_ != nullptr) {
    consolidatePosition();
    commitInteraction(tr("Move Shape"));
  }
}

ImageScene* ShapeItem::imageScene() const noexcept { return dynamic_cast<ImageScene*>(scene()); }

void ShapeItem::rebuildHandles() {
  for (ShapeHandleItem* handle : handles_) {
    delete handle;
  }
  handles_.clear();
  handles_.reserve(shape_->handles().size());
  for (const ShapeHandle& handle : shape_->handles()) {
    handles_.push_back(new ShapeHandleItem{*this, handle});
  }
  updateHandles();
  updateHandleVisibility();
}

void ShapeItem::updateHandles() {
  const std::span<const ShapeHandle> modelHandles = shape_->handles();
  if (handles_.size() != modelHandles.size()) {
    rebuildHandles();
    return;
  }
  for (std::size_t index = 0; index < handles_.size(); ++index) {
    handles_[index]->setPos(shape_->handleCenter(modelHandles[index]));
  }
}

void ShapeItem::updateHandleVisibility() {
  for (std::size_t index = 0; index < handles_.size(); ++index) {
    const bool active = !activeHandle_.has_value() || shape_->handles()[index] == *activeHandle_;
    handles_[index]->setVisible(isSelected() && active);
  }
}

void ShapeItem::beginHandleInteraction(const ShapeHandle& handle, Qt::MouseButton button,
                                       const QPointF& scenePosition) {
  setSelected(true);
  interactionStart_ = std::make_unique<ShapeItemGeometry>(captureGeometry());
  activeHandle_ = handle;
  updateHandleVisibility();
  if (button == Qt::LeftButton) {
    handleInteraction_ = HandleInteraction::Resize;
  } else if (button == Qt::RightButton) {
    handleInteraction_ = HandleInteraction::Rotate;
    rotationCenter_ = mapToScene(shape_->boundingRect().center());
    initialMouseAngle_ = mouseAngle(rotationCenter_, scenePosition);
    initialRotation_ = shape_->rotationDegrees();
    if (ImageScene* currentScene = imageScene(); currentScene != nullptr) {
      currentScene->suppressNextContextMenu();
    }
  }
}

void ShapeItem::updateHandleInteraction(const QPointF& scenePosition) {
  if (interactionStart_ == nullptr) {
    return;
  }
  if (!activeHandle_.has_value()) {
    return;
  }
  const ShapeHandle activeHandle = activeHandle_.value();
  prepareGeometryChange();
  if (handleInteraction_ == HandleInteraction::Resize) {
    const QPointF localPosition = mapFromScene(scenePosition);
    const ImageScene* currentScene = imageScene();
    Q_ASSERT(currentScene != nullptr);
    if (currentScene == nullptr) {
      return;
    }
    const QRectF localLimits = mapRectFromParent(currentScene->imageBounds());
    shape_->resize(*interactionStart_->shape, activeHandle, localPosition, localLimits);
  } else if (handleInteraction_ == HandleInteraction::Rotate) {
    const qreal angle = mouseAngle(rotationCenter_, scenePosition);
    shape_->restoreGeometry(*interactionStart_->shape);
    shape_->setRotationDegrees(initialRotation_ + angle - initialMouseAngle_);
  }
  updateHandles();
  update();
}

void ShapeItem::finishHandleInteraction() {
  if (handleInteraction_ == HandleInteraction::None) {
    return;
  }
  const QString undoText =
      handleInteraction_ == HandleInteraction::Resize ? tr("Resize Shape") : tr("Rotate Shape");
  commitInteraction(undoText);
  handleInteraction_ = HandleInteraction::None;
  activeHandle_.reset();
  updateHandleVisibility();
}

void ShapeItem::commitInteraction(const QString& undoText) {
  if (interactionStart_ == nullptr) {
    return;
  }
  ShapeItemGeometry before = std::move(*interactionStart_);
  interactionStart_.reset();
  if (ImageScene* currentScene = imageScene(); currentScene != nullptr) {
    currentScene->commitTransform(*this, std::move(before), captureGeometry(), undoText);
  }
}

void ShapeItem::consolidatePosition() {
  if (pos().isNull()) {
    return;
  }
  // Keep the domain geometry in image coordinates for ROI export; item position is only the live
  // movement mechanism provided by Graphics View.
  prepareGeometryChange();
  shape_->moveBy(pos());
  setPos(QPointF{});
  updateHandles();
  update();
}

} // namespace quickshot
