#pragma once

#include "quickshot/shapes/shape.hpp"

#include <QGraphicsObject>
#include <QPointF>
#include <QString>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

class QGraphicsSceneMouseEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace quickshot {

class ImageScene;
class ShapeHandleItem;

struct ShapeItemGeometry {
  QPointF position;
  std::unique_ptr<ShapeGeometry> shape;
};

class ShapeItem final : public QGraphicsObject {
public:
  explicit ShapeItem(std::unique_ptr<Shape> shape, QGraphicsItem* parent = nullptr);
  ~ShapeItem() override;

  ShapeItem(const ShapeItem&) = delete;
  ShapeItem& operator=(const ShapeItem&) = delete;
  ShapeItem(ShapeItem&&) = delete;
  ShapeItem& operator=(ShapeItem&&) = delete;

  [[nodiscard]] QRectF boundingRect() const override;
  [[nodiscard]] QPainterPath shape() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
             QWidget* widget = nullptr) override;

  [[nodiscard]] const Shape& model() const noexcept;
  [[nodiscard]] Shape& model() noexcept;
  [[nodiscard]] std::unique_ptr<ShapeItem> clone() const;
  [[nodiscard]] QPainterPath imagePath() const;
  [[nodiscard]] ShapeItemGeometry captureGeometry() const;
  void restoreGeometry(const ShapeItemGeometry& geometry);
  void updateCreation(const QPointF& origin, const QRectF& imageBounds, const QPointF& imagePoint);
  void appendCreationPoint(const QPointF& point);
  void setCreationPreview(const QPointF& point);
  void finishCreation();

protected:
  [[nodiscard]] QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
  friend class ShapeHandleItem;

  enum class HandleInteraction : std::uint8_t { None, Resize, Rotate };

  [[nodiscard]] ImageScene* imageScene() const noexcept;
  void rebuildHandles();
  void updateHandles();
  void updateHandleVisibility();
  void beginHandleInteraction(const ShapeHandle& handle, Qt::MouseButton button,
                              const QPointF& scenePosition);
  void updateHandleInteraction(const QPointF& scenePosition);
  void finishHandleInteraction();
  void commitInteraction(const QString& undoText);
  void consolidatePosition();

  std::unique_ptr<Shape> shape_;
  std::vector<ShapeHandleItem*> handles_;
  std::unique_ptr<ShapeItemGeometry> interactionStart_;
  std::optional<ShapeHandle> activeHandle_;
  QPointF rotationCenter_;
  qreal initialMouseAngle_ = 0.0;
  qreal initialRotation_ = 0.0;
  HandleInteraction handleInteraction_ = HandleInteraction::None;
};

} // namespace quickshot
