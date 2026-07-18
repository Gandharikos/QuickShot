#pragma once

#include <QGraphicsView>
#include <QImage>
#include <QPointF>
#include <QSize>
#include <QStringList>
#include <QUndoGroup>
#include <QUndoStack>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

class QAction;
class QContextMenuEvent;
class QEvent;
class QMouseEvent;
class QWheelEvent;

namespace quickshot {

class ImageDocument;
class ImageScene;
class Shape;
class ShapeItem;
enum class ShapeType : std::uint8_t;

class CanvasView final : public QGraphicsView {
  Q_OBJECT

public:
  explicit CanvasView(QWidget* parent = nullptr);
  ~CanvasView() override;

  [[nodiscard]] bool loadImage(const QString& fileName);
  [[nodiscard]] QStringList loadImages(const QStringList& fileNames);
  [[nodiscard]] bool hasImage() const noexcept;
  [[nodiscard]] qsizetype imageCount() const noexcept;
  [[nodiscard]] qsizetype currentImageIndex() const noexcept;
  [[nodiscard]] QString imagePathAt(qsizetype index) const;
  [[nodiscard]] QImage thumbnailAt(qsizetype index, const QSize& size) const;
  [[nodiscard]] qreal zoomFactor() const noexcept;
  [[nodiscard]] qreal imageRotationDegrees() const noexcept;
  [[nodiscard]] QSize sizeHint() const override;
  [[nodiscard]] qsizetype shapeCount() const noexcept;
  [[nodiscard]] qsizetype shapeCountAt(qsizetype imageIndex) const noexcept;
  [[nodiscard]] const ::quickshot::Shape* shapeAt(qsizetype index) const;
  [[nodiscard]] QUndoStack& undoStack() noexcept;
  [[nodiscard]] const QUndoStack& undoStack() const noexcept;
  [[nodiscard]] QUndoGroup& undoGroup() noexcept;
  [[nodiscard]] const QUndoGroup& undoGroup() const noexcept;
  void setCurrentImageIndex(qsizetype index);
  void removeImage(qsizetype index);
  void removeCurrentImage();
  void clearImageShapes(qsizetype index);
  void clearCurrentImageShapes();
  void setZoomFactor(qreal factor);
  void setCreationMode(ShapeType type, bool enabled);
  void rotateLeft();
  void rotateRight();

signals:
  void imageAvailabilityChanged(bool available);
  void zoomFactorChanged(qreal factor);
  void imageCollectionChanged();
  void currentShapeAvailabilityChanged(bool available);
  void currentImageChanged(qsizetype index);
  void imageThumbnailChanged(qsizetype index);
  void cursorImagePositionChanged(const QPointF& position);
  void cursorLeftImage();
  void imageRotationChanged(qreal degrees);

protected:
  void contextMenuEvent(QContextMenuEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

private:
  [[nodiscard]] ImageDocument* currentDocument() noexcept;
  [[nodiscard]] const ImageDocument* currentDocument() const noexcept;
  [[nodiscard]] ImageScene* imageScene() noexcept;
  [[nodiscard]] const ImageScene* imageScene() const noexcept;
  void initializeContextActions();
  void saveSelectedRoi();
  void batchSaveSelectedRoi();
  void cloneSelectedShape();
  void deleteSelectedShape();
  void saveAllRois();
  void batchSaveAllRois();
  void deleteAllShapes();
  void pushCommand(std::unique_ptr<QUndoCommand> command);
  void saveRois(const std::vector<ShapeItem*>& targets);
  void batchSaveRois(const std::vector<ShapeItem*>& targets);
  void rotateImage(qreal degrees);

  std::vector<std::unique_ptr<ImageDocument>> documents_;
  qsizetype currentImageIndex_ = -1;
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
  std::optional<ShapeType> creationType_;
  qreal zoomFactor_ = 1.0;
};

} // namespace quickshot
