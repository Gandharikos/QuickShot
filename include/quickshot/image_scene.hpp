#pragma once

#include "quickshot/shape_item.hpp"
#include "quickshot/shapes/shape.hpp"

#include <QGraphicsScene>
#include <QImage>
#include <QString>
#include <memory>
#include <optional>
#include <vector>

class QGraphicsSceneContextMenuEvent;
class QGraphicsSceneMouseEvent;
class QUndoStack;
class QUndoCommand;

namespace quickshot {

class ImageItem;

class ImageScene final : public QGraphicsScene {
  Q_OBJECT

public:
  explicit ImageScene(QImage image, QObject* parent = nullptr);
  ~ImageScene() override;

  void setUndoStack(QUndoStack& undoStack) noexcept;
  [[nodiscard]] const QImage& image() const noexcept;
  void setImage(QImage image);
  [[nodiscard]] QRectF imageBounds() const noexcept;
  void setCreationMode(ShapeType type, bool enabled);
  void cancelCreation();

  [[nodiscard]] qsizetype shapeCount() const noexcept;
  [[nodiscard]] ShapeItem* shapeItemAt(qsizetype index) const;
  [[nodiscard]] ShapeItem* shapeItemAt(const QPointF& scenePosition) const;
  [[nodiscard]] ShapeItem* selectedShapeItem() const;
  [[nodiscard]] std::vector<ShapeItem*> shapeItems() const;

  [[nodiscard]] std::unique_ptr<ShapeItem> makeOffsetClone(const ShapeItem& source) const;
  void addShapeItem(ShapeItem& item);
  [[nodiscard]] std::unique_ptr<ShapeItem> takeShapeItem(ShapeItem& item);
  void commitTransform(ShapeItem& item, ShapeItemGeometry before, ShapeItemGeometry after,
                       const QString& text);
  void suppressNextContextMenu() noexcept;
  [[nodiscard]] bool consumeContextMenuSuppression() noexcept;
  void applyImageTransform(const QTransform& transformation);

protected:
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
  void beginCreation(const QPointF& position);
  void finishCreation(bool suppressContextMenu);
  void pushCommand(std::unique_ptr<QUndoCommand> command);

  QImage image_;
  ImageItem* imageItem_ = nullptr;
  QUndoStack* undoStack_ = nullptr;
  std::optional<ShapeType> creationType_;
  ShapeItem* provisionalShape_ = nullptr;
  ShapeItem* previousSelection_ = nullptr;
  QPointF creationOrigin_;
  qreal nextShapeZ_ = 1.0;
  bool suppressContextMenu_ = false;
};

} // namespace quickshot
