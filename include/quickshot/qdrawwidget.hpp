#pragma once

#include "quickshot/drag_controller.hpp"

#include <QAbstractScrollArea>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QUndoStack>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

class QContextMenuEvent;
class QAction;
class QMouseEvent;
class QPainter;
class QPaintEvent;
class QResizeEvent;
class QString;
class QWheelEvent;

namespace quickshot {

enum class ShapeType : std::uint8_t;
class Shape;
class ShapeCommand;

class QDrawWidget final : public QAbstractScrollArea {
  Q_OBJECT

public:
  explicit QDrawWidget(QWidget* parent = nullptr);
  ~QDrawWidget() override;

  [[nodiscard]] bool loadImage(const QString& fileName);
  [[nodiscard]] bool hasImage() const noexcept;
  [[nodiscard]] qreal zoomFactor() const noexcept;
  [[nodiscard]] QSize sizeHint() const override;
  [[nodiscard]] qsizetype shapeCount() const noexcept;
  [[nodiscard]] const ::quickshot::Shape* shapeAt(qsizetype index) const;
  [[nodiscard]] QUndoStack& undoStack() noexcept;
  [[nodiscard]] const QUndoStack& undoStack() const noexcept;
  void setZoomFactor(qreal factor);
  void setCreationMode(ShapeType type, bool enabled);
  void rotateLeft();
  void rotateRight();

signals:
  void imageAvailabilityChanged(bool available);
  void zoomFactorChanged(qreal factor);

protected:
  void contextMenuEvent(QContextMenuEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

private:
  friend class ShapeCommand;

  [[nodiscard]] QPointF imagePosition(const QPointF& viewportPosition) const;
  [[nodiscard]] QRectF imageBounds() const;
  [[nodiscard]] ::quickshot::Shape* shapeAt(const QPointF& point) const;
  [[nodiscard]] std::optional<ShapeHandle> handleAt(const QPointF& point) const;
  [[nodiscard]] QRectF handleRect(const ::quickshot::Shape& shape, const ShapeHandle& handle) const;
  [[nodiscard]] QRectF constrainedMove(const QRectF& bounds, const QPointF& offset) const;
  void initializeContextActions();
  void saveSelectedRoi();
  void cloneSelectedShape();
  void deleteSelectedShape();
  void saveAllRois();
  void deleteAllShapes();
  void updateHoverCursor(const QPointF& point);
  void drawSelectionHandles(QPainter& painter) const;
  [[nodiscard]] std::unique_ptr<::quickshot::Shape>
  makeOffsetClone(const ::quickshot::Shape& shape) const;
  void pushCommand(std::unique_ptr<QUndoCommand> command);
  void completeDrag(DragCompletion completion);
  void cancelDrag();
  void clearUndoHistoryForUntrackedEdit();
  void saveRois(const std::vector<const ::quickshot::Shape*>& targets);
  void rotateImage(qreal degrees);
  [[nodiscard]] QSize scaledImageSize() const;
  void updateScrollBars();

  QImage image_;
  std::vector<std::unique_ptr<::quickshot::Shape>> shapes_;
  ::quickshot::Shape* selectedShape_ = nullptr;
  std::optional<ShapeType> creationType_;
  DragController dragController_;
  QUndoStack undoStack_;
  QAction* saveRoiAction_ = nullptr;
  QAction* cloneShapeAction_ = nullptr;
  QAction* deleteShapeAction_ = nullptr;
  QAction* saveAllRoisAction_ = nullptr;
  QAction* deleteAllShapesAction_ = nullptr;
  QString lastSaveDirectory_;
  qreal zoomFactor_ = 1.0;
  bool suppressContextMenu_ = false;
};

} // namespace quickshot
