#pragma once

#include "quickshot/drag_controller.hpp"

#include <QAbstractScrollArea>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QUndoGroup>
#include <QUndoStack>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

class QContextMenuEvent;
class QAction;
class QEvent;
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
  [[nodiscard]] QStringList loadImages(const QStringList& fileNames);
  [[nodiscard]] bool hasImage() const noexcept;
  [[nodiscard]] qsizetype imageCount() const noexcept;
  [[nodiscard]] qsizetype currentImageIndex() const noexcept;
  [[nodiscard]] QString imagePathAt(qsizetype index) const;
  [[nodiscard]] QImage thumbnailAt(qsizetype index, const QSize& size) const;
  [[nodiscard]] qreal zoomFactor() const noexcept;
  [[nodiscard]] QSize sizeHint() const override;
  [[nodiscard]] qsizetype shapeCount() const noexcept;
  [[nodiscard]] const ::quickshot::Shape* shapeAt(qsizetype index) const;
  [[nodiscard]] QUndoStack& undoStack() noexcept;
  [[nodiscard]] const QUndoStack& undoStack() const noexcept;
  [[nodiscard]] QUndoGroup& undoGroup() noexcept;
  [[nodiscard]] const QUndoGroup& undoGroup() const noexcept;
  void setCurrentImageIndex(qsizetype index);
  void setZoomFactor(qreal factor);
  void setCreationMode(ShapeType type, bool enabled);
  void rotateLeft();
  void rotateRight();

signals:
  void imageAvailabilityChanged(bool available);
  void zoomFactorChanged(qreal factor);
  void imageCollectionChanged();
  void currentImageChanged(qsizetype index);
  void imageThumbnailChanged(qsizetype index);
  void cursorImagePositionChanged(const QPointF& position);
  void cursorLeftImage();

protected:
  void contextMenuEvent(QContextMenuEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

private:
  friend class ShapeCommand;

  class ImageDocument;

  [[nodiscard]] QPointF imagePosition(const QPointF& viewportPosition) const;
  [[nodiscard]] QRectF imageBounds() const;
  [[nodiscard]] ::quickshot::Shape* shapeAt(const QPointF& point) const;
  [[nodiscard]] std::optional<ShapeHandle> handleAt(const QPointF& point) const;
  [[nodiscard]] QRectF handleRect(const ::quickshot::Shape& shape, const ShapeHandle& handle) const;
  [[nodiscard]] QRectF constrainedMove(const QRectF& bounds, const QPointF& offset) const;
  void initializeContextActions();
  void saveSelectedRoi();
  void batchSaveSelectedRoi();
  void cloneSelectedShape();
  void deleteSelectedShape();
  void saveAllRois();
  void batchSaveAllRois();
  void deleteAllShapes();
  void updateHoverCursor(const QPointF& point);
  void drawSelectionHandles(QPainter& painter) const;
  [[nodiscard]] std::unique_ptr<::quickshot::Shape>
  makeOffsetClone(const ::quickshot::Shape& shape) const;
  void pushCommand(std::unique_ptr<QUndoCommand> command);
  void completeDrag(DragCompletion completion);
  void cancelDrag();
  void clearUndoHistoryForUntrackedEdit();
  void storeCurrentDocument();
  [[nodiscard]] const QImage& documentImage(qsizetype index) const;
  void saveRois(const std::vector<const ::quickshot::Shape*>& targets);
  void batchSaveRois(const std::vector<const ::quickshot::Shape*>& targets);
  void rotateImage(qreal degrees);
  [[nodiscard]] QSize scaledImageSize() const;
  void updateScrollBars();

  QImage image_;
  std::vector<std::unique_ptr<::quickshot::Shape>> shapes_;
  ::quickshot::Shape* selectedShape_ = nullptr;
  std::vector<std::unique_ptr<ImageDocument>> documents_;
  qsizetype currentImageIndex_ = -1;
  std::optional<ShapeType> creationType_;
  DragController dragController_;
  QUndoGroup undoGroup_;
  QUndoStack fallbackUndoStack_;
  QAction* saveRoiAction_ = nullptr;
  QAction* batchSaveRoiAction_ = nullptr;
  QAction* cloneShapeAction_ = nullptr;
  QAction* deleteShapeAction_ = nullptr;
  QAction* saveAllRoisAction_ = nullptr;
  QAction* batchSaveAllRoisAction_ = nullptr;
  QAction* deleteAllShapesAction_ = nullptr;
  QString lastSaveDirectory_;
  qreal zoomFactor_ = 1.0;
  bool suppressContextMenu_ = false;
};

} // namespace quickshot
