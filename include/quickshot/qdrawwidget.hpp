#pragma once

#include <QAbstractScrollArea>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QTransform>
#include <memory>
#include <optional>
#include <vector>

class QContextMenuEvent;
class QMouseEvent;
class QPainter;
class QPaintEvent;
class QResizeEvent;
class QString;
class QWheelEvent;

namespace quickshot {

enum class HandlePosition;
enum class ShapeType;
class Shape;

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
  enum class DragMode { None, Create, Move, Resize, Rotate };

  [[nodiscard]] QPointF imagePosition(const QPointF& viewportPosition) const;
  [[nodiscard]] QRectF imageBounds() const;
  [[nodiscard]] ::quickshot::Shape* shapeAt(const QPointF& point) const;
  [[nodiscard]] std::optional<HandlePosition> handleAt(const QPointF& point) const;
  [[nodiscard]] QRectF handleRect(const ::quickshot::Shape& shape, HandlePosition position) const;
  [[nodiscard]] QRectF constrainedMove(const QRectF& bounds, const QPointF& offset) const;
  [[nodiscard]] QRectF resizedBounds(const QRectF& bounds, HandlePosition handle,
                                     const QPointF& point) const;
  void updateHoverCursor(const QPointF& point);
  void updateDraggedShape(const QPointF& point);
  void drawSelectionHandles(QPainter& painter) const;
  void cloneShape(const ::quickshot::Shape& shape);
  void deleteShape(const ::quickshot::Shape& shape);
  void deleteAllShapes();
  void saveRois(const std::vector<const ::quickshot::Shape*>& targets);
  void rotateImage(qreal degrees);
  [[nodiscard]] QSize scaledImageSize() const;
  void updateScrollBars();

  QImage image_;
  std::vector<std::unique_ptr<::quickshot::Shape>> shapes_;
  ::quickshot::Shape* selectedShape_ = nullptr;
  std::optional<ShapeType> creationType_;
  DragMode dragMode_ = DragMode::None;
  std::optional<HandlePosition> activeHandle_;
  QPointF dragStart_;
  QRectF dragStartBounds_;
  QTransform dragStartImageToShape_;
  QPointF resizeAnchorImage_;
  qreal dragStartRotation_ = 0.0;
  qreal dragStartMouseAngle_ = 0.0;
  QString lastSaveDirectory_;
  qreal zoomFactor_ = 1.0;
  bool suppressContextMenu_ = false;
};

} // namespace quickshot
