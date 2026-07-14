#include "quickshot/qdrawwidget.hpp"

#include "quickshot/ellipse.hpp"
#include "quickshot/rectangle.hpp"
#include "quickshot/roi_exporter.hpp"
#include "quickshot/shape.hpp"

#include <QAction>
#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QImageReader>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPointF>
#include <QRectF>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSettings>
#include <QSize>
#include <QStandardPaths>
#include <QSvgRenderer>
#include <QTransform>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>
#include <array>
#include <cmath>
#include <ranges>
#include <utility>

namespace quickshot {
namespace {

constexpr qreal minimumZoom = 0.1;
constexpr qreal maximumZoom = 8.0;
constexpr qreal zoomPerWheelStep = 1.1;
constexpr qreal wheelStepAngle = 120.0;
constexpr qreal handleSize = 8.0;
constexpr qreal minimumShapeSize = 1.0;
constexpr qreal cloneOffset = 10.0;
const QColor rotationHandleColor{0, 200, 83};

QTransform rotationTransform(const QRectF& bounds, qreal degrees) {
  const QPointF center = bounds.center();
  QTransform transformation;
  transformation.translate(center.x(), center.y());
  transformation.rotate(degrees);
  transformation.translate(-center.x(), -center.y());
  return transformation;
}

HandlePosition oppositeHandle(HandlePosition position) {
  switch (position) {
  case HandlePosition::TopLeft:
    return HandlePosition::BottomRight;
  case HandlePosition::Top:
    return HandlePosition::Bottom;
  case HandlePosition::TopRight:
    return HandlePosition::BottomLeft;
  case HandlePosition::Right:
    return HandlePosition::Left;
  case HandlePosition::BottomRight:
    return HandlePosition::TopLeft;
  case HandlePosition::Bottom:
    return HandlePosition::Top;
  case HandlePosition::BottomLeft:
    return HandlePosition::TopRight;
  case HandlePosition::Left:
    return HandlePosition::Right;
  }

  return HandlePosition::BottomRight;
}

qreal mouseAngle(const QPointF& center, const QPointF& point) {
  return qRadiansToDegrees(std::atan2(point.y() - center.y(), point.x() - center.x()));
}

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

QString pngFileName(const QString& selectedFileName) {
  const QFileInfo selectedFile{selectedFileName};
  if (selectedFile.suffix().compare(QStringLiteral("png"), Qt::CaseInsensitive) == 0) {
    return selectedFile.absoluteFilePath();
  }

  const QString baseName =
      selectedFile.suffix().isEmpty() ? selectedFile.fileName() : selectedFile.completeBaseName();
  return selectedFile.dir().filePath(baseName + QStringLiteral(".png"));
}

QString numberedPngFileName(const QString& baseFileName, std::size_t index) {
  const QFileInfo baseFile{baseFileName};
  const QString numberedName = QStringLiteral("%1_%2.png")
                                   .arg(baseFile.completeBaseName())
                                   .arg(static_cast<qulonglong>(index) + 1ULL, 3, 10, QChar{'0'});
  return baseFile.dir().filePath(numberedName);
}

} // namespace

QDrawWidget::QDrawWidget(QWidget* parent) : QAbstractScrollArea(parent) {
  setFrameShape(QFrame::NoFrame);
  setFocusPolicy(Qt::StrongFocus);
  viewport()->setAutoFillBackground(false);
  viewport()->setMouseTracking(true);
  qApp->installEventFilter(this);

  QSettings settings;
  lastSaveDirectory_ = settings.value(QStringLiteral("roi/lastSaveDirectory")).toString();
  if (lastSaveDirectory_.isEmpty()) {
    lastSaveDirectory_ = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
  }
  if (lastSaveDirectory_.isEmpty()) {
    lastSaveDirectory_ = QDir::homePath();
  }
}

QDrawWidget::~QDrawWidget() {
  if (qApp != nullptr) {
    qApp->removeEventFilter(this);
  }
}

bool QDrawWidget::loadImage(const QString& fileName) {
  QImageReader reader(fileName);
  reader.setAutoTransform(true);

  QImage image = reader.read();
  if (image.isNull()) {
    return false;
  }

  image_ = std::move(image);
  shapes_.clear();
  selectedShape_ = nullptr;
  dragMode_ = DragMode::None;
  activeHandle_.reset();
  const qreal previousZoom = zoomFactor_;
  zoomFactor_ = 1.0;
  horizontalScrollBar()->setValue(0);
  verticalScrollBar()->setValue(0);
  updateScrollBars();
  viewport()->update();
  if (!qFuzzyCompare(previousZoom, zoomFactor_)) {
    emit zoomFactorChanged(zoomFactor_);
  }
  emit imageAvailabilityChanged(true);
  return true;
}

bool QDrawWidget::hasImage() const noexcept { return !image_.isNull(); }

qreal QDrawWidget::zoomFactor() const noexcept { return zoomFactor_; }

QSize QDrawWidget::sizeHint() const { return {640, 360}; }

qsizetype QDrawWidget::shapeCount() const noexcept {
  return static_cast<qsizetype>(shapes_.size());
}

const ::quickshot::Shape* QDrawWidget::shapeAt(qsizetype index) const {
  if (index < 0) {
    return nullptr;
  }

  const auto position = static_cast<std::size_t>(index);
  return position < shapes_.size() ? shapes_[position].get() : nullptr;
}

void QDrawWidget::setZoomFactor(qreal factor) {
  const qreal boundedFactor = std::clamp(factor, minimumZoom, maximumZoom);
  if (qFuzzyCompare(zoomFactor_, boundedFactor)) {
    return;
  }

  zoomFactor_ = boundedFactor;
  updateScrollBars();
  viewport()->update();
  emit zoomFactorChanged(zoomFactor_);
}

void QDrawWidget::setRectangleCreationMode(bool enabled) {
  setCreationMode(CreationMode::Rectangle, enabled);
}

void QDrawWidget::setEllipseCreationMode(bool enabled) {
  setCreationMode(CreationMode::Ellipse, enabled);
}

void QDrawWidget::rotateLeft() { rotateImage(-90.0); }

void QDrawWidget::rotateRight() { rotateImage(90.0); }

void QDrawWidget::contextMenuEvent(QContextMenuEvent* event) {
  if (suppressContextMenu_) {
    suppressContextMenu_ = false;
    event->accept();
    return;
  }

  const QPointF point = imagePosition(event->pos());
  ::quickshot::Shape* targetShape = shapeAt(point);

  QMenu menu{viewport()};
  if (targetShape != nullptr) {
    selectedShape_ = targetShape;
    viewport()->update();

    QAction* saveAction = menu.addAction(tr("Save ROI"));
    saveAction->setObjectName("saveRoiAction");
    QAction* cloneAction = menu.addAction(tr("Clone"));
    cloneAction->setObjectName("cloneShapeAction");
    QAction* deleteAction = menu.addAction(tr("Delete"));
    deleteAction->setObjectName("deleteShapeAction");

    const QAction* selectedAction = menu.exec(event->globalPos());
    if (selectedAction == saveAction) {
      saveRois({targetShape});
    } else if (selectedAction == cloneAction) {
      cloneShape(*targetShape);
    } else if (selectedAction == deleteAction) {
      deleteShape(*targetShape);
    }
  } else {
    QAction* saveAllAction = menu.addAction(tr("Save All"));
    saveAllAction->setObjectName("saveAllRoisAction");
    saveAllAction->setEnabled(!shapes_.empty());
    QAction* deleteAllAction = menu.addAction(tr("Delete All"));
    deleteAllAction->setObjectName("deleteAllShapesAction");
    deleteAllAction->setEnabled(!shapes_.empty());

    const QAction* selectedAction = menu.exec(event->globalPos());
    if (selectedAction == saveAllAction) {
      std::vector<const ::quickshot::Shape*> targets;
      targets.reserve(shapes_.size());
      for (const std::unique_ptr<::quickshot::Shape>& shape : shapes_) {
        targets.push_back(shape.get());
      }
      saveRois(targets);
    } else if (selectedAction == deleteAllAction) {
      deleteAllShapes();
    }
  }

  event->accept();
}

bool QDrawWidget::eventFilter(QObject* watched, QEvent* event) {
  if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
    const auto* keyEvent = static_cast<QKeyEvent*>(event);
    if (keyEvent->key() == Qt::Key_Alt && !keyEvent->isAutoRepeat()) {
      setRotationMode(event->type() == QEvent::KeyPress);
    }
  } else if (event->type() == QEvent::ApplicationDeactivate) {
    setRotationMode(false);
    if (dragMode_ == DragMode::Rotate) {
      dragMode_ = DragMode::None;
      activeHandle_.reset();
      suppressContextMenu_ = false;
      viewport()->update();
    }
  }

  return QAbstractScrollArea::eventFilter(watched, event);
}

void QDrawWidget::mouseMoveEvent(QMouseEvent* event) {
  const QPointF point = imagePosition(event->position());
  if (dragMode_ != DragMode::None) {
    updateDraggedShape(point);
    viewport()->update();
    event->accept();
    return;
  }

  updateHoverCursor(point);
  QAbstractScrollArea::mouseMoveEvent(event);
}

void QDrawWidget::mousePressEvent(QMouseEvent* event) {
  if (image_.isNull()) {
    QAbstractScrollArea::mousePressEvent(event);
    return;
  }

  const QPointF point = imagePosition(event->position());
  if (event->button() == Qt::RightButton &&
      (rotationMode_ || event->modifiers().testFlag(Qt::AltModifier))) {
    setRotationMode(true);
    if (::quickshot::Shape* rotationShape = shapeAtRotationHandle(point);
        rotationShape != nullptr) {
      selectedShape_ = rotationShape;
      dragMode_ = DragMode::Rotate;
      activeHandle_ = HandlePosition::TopLeft;
      dragStartRotation_ = rotationShape->rotationDegrees();
      dragStartMouseAngle_ = mouseAngle(rotationShape->boundingRect().center(), point);
      suppressContextMenu_ = true;
      viewport()->setCursor(rotationCursor());
      viewport()->update();
      event->accept();
      return;
    }
  }

  if (event->button() != Qt::LeftButton) {
    if (event->button() == Qt::RightButton) {
      suppressContextMenu_ = false;
    }
    QAbstractScrollArea::mousePressEvent(event);
    return;
  }

  if (!imageBounds().contains(point)) {
    QAbstractScrollArea::mousePressEvent(event);
    return;
  }

  if (const std::optional<HandlePosition> handle = handleAt(point); handle.has_value()) {
    dragMode_ = DragMode::Resize;
    activeHandle_ = handle;
    dragStart_ = point;
    dragStartBounds_ = selectedShape_->boundingRect();
    dragStartImageToShape_ = selectedShape_->imageTransform().inverted();
    resizeAnchorImage_ = selectedShape_->handleCenter(SizeHandle{oppositeHandle(*activeHandle_)});
    dragStartRotation_ = selectedShape_->rotationDegrees();
    viewport()->update();
    event->accept();
    return;
  }

  if (::quickshot::Shape* hitShape = shapeAt(point); hitShape != nullptr) {
    selectedShape_ = hitShape;
    dragMode_ = DragMode::Move;
    dragStart_ = point;
    dragStartBounds_ = selectedShape_->boundingRect();
    viewport()->update();
    event->accept();
    return;
  }

  if (creationMode_ != CreationMode::None) {
    std::unique_ptr<::quickshot::Shape> shape;
    const QRectF initialBounds{point, point};
    if (creationMode_ == CreationMode::Rectangle) {
      shape = std::make_unique<Rectangle>(initialBounds);
    } else {
      shape = std::make_unique<Ellipse>(initialBounds);
    }

    selectedShape_ = shape.get();
    shapes_.push_back(std::move(shape));
    dragMode_ = DragMode::Create;
    dragStart_ = point;
    dragStartBounds_ = initialBounds;
    viewport()->update();
    event->accept();
    return;
  }

  selectedShape_ = nullptr;
  viewport()->update();
  event->accept();
}

void QDrawWidget::mouseReleaseEvent(QMouseEvent* event) {
  const bool completesLeftDrag = event->button() == Qt::LeftButton && dragMode_ != DragMode::Rotate;
  const bool completesRotation =
      event->button() == Qt::RightButton && dragMode_ == DragMode::Rotate;
  if (dragMode_ == DragMode::None || (!completesLeftDrag && !completesRotation)) {
    QAbstractScrollArea::mouseReleaseEvent(event);
    return;
  }

  if (dragMode_ == DragMode::Create && selectedShape_ != nullptr) {
    const QRectF bounds = selectedShape_->boundingRect();
    if (bounds.width() < minimumShapeSize || bounds.height() < minimumShapeSize) {
      shapes_.pop_back();
      selectedShape_ = nullptr;
    }
  }

  dragMode_ = DragMode::None;
  activeHandle_.reset();
  updateHoverCursor(imagePosition(event->position()));
  viewport()->update();
  event->accept();
}

void QDrawWidget::paintEvent(QPaintEvent* event) {
  QAbstractScrollArea::paintEvent(event);

  QPainter painter(viewport());
  const QRectF viewportRect{0.0, 0.0, static_cast<qreal>(viewport()->width()),
                            static_cast<qreal>(viewport()->height())};
  // Uncovered viewport pixels remain black at every zoom level.
  painter.fillRect(viewportRect, Qt::black);

  if (image_.isNull()) {
    return;
  }

  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  // Scrollbar values are offsets in scaled content coordinates, so translate in the opposite
  // direction before scaling the image uniformly.
  painter.translate(-horizontalScrollBar()->value(), -verticalScrollBar()->value());
  painter.scale(zoomFactor_, zoomFactor_);
  painter.drawImage(QPointF{0.0, 0.0}, image_);

  painter.setRenderHint(QPainter::Antialiasing);
  QPen shapePen;
  shapePen.setCosmetic(true);
  shapePen.setWidth(2);
  painter.setBrush(Qt::NoBrush);
  for (const std::unique_ptr<::quickshot::Shape>& shape : shapes_) {
    shapePen.setColor(shape.get() == selectedShape_ ? Qt::red : Qt::white);
    painter.setPen(shapePen);
    shape->draw(painter);
  }
  drawSelectionHandles(painter);
  drawRotationHandles(painter);
}

void QDrawWidget::resizeEvent(QResizeEvent* event) {
  QAbstractScrollArea::resizeEvent(event);
  // A resized viewport changes the visible page and therefore the valid scroll range.
  updateScrollBars();
}

void QDrawWidget::wheelEvent(QWheelEvent* event) {
  // y() is vertical wheel rotation; x() carries horizontal input from tilt wheels or touchpads.
  const int angleDelta = event->angleDelta().y();
  if (image_.isNull() || !event->modifiers().testFlag(Qt::ControlModifier) || angleDelta == 0) {
    QAbstractScrollArea::wheelEvent(event);
    return;
  }

  const qreal previousZoom = zoomFactor_;
  // Qt reports angleDelta in eighths of a degree, so a typical 15-degree notch is 120 units;
  // keeping a fractional step also supports high-resolution wheels.
  const qreal wheelSteps = static_cast<qreal>(angleDelta) / wheelStepAngle;
  // Multiplication gives every notch the same relative visual change, while clamp keeps the
  // resulting scale within the supported 10%-800% range.
  setZoomFactor(previousZoom * std::pow(zoomPerWheelStep, wheelSteps));

  // Compare floating-point values approximately; clamp can leave the scale unchanged at a limit.
  if (qFuzzyCompare(zoomFactor_, previousZoom)) {
    event->accept();
    return;
  }

  // Keeping the scrollbar offsets unchanged makes the image's top-left corner the zoom anchor.
  event->accept();
}

QPointF QDrawWidget::imagePosition(const QPointF& viewportPosition) const {
  return {(viewportPosition.x() + static_cast<qreal>(horizontalScrollBar()->value())) / zoomFactor_,
          (viewportPosition.y() + static_cast<qreal>(verticalScrollBar()->value())) / zoomFactor_};
}

QRectF QDrawWidget::imageBounds() const {
  return {0.0, 0.0, static_cast<qreal>(image_.width()), static_cast<qreal>(image_.height())};
}

::quickshot::Shape* QDrawWidget::shapeAt(const QPointF& point) const {
  for (auto shape = shapes_.rbegin(); shape != shapes_.rend(); ++shape) {
    if ((*shape)->contains(point)) {
      return shape->get();
    }
  }
  return nullptr;
}

::quickshot::Shape* QDrawWidget::shapeAtRotationHandle(const QPointF& point) const {
  for (auto shape = shapes_.rbegin(); shape != shapes_.rend(); ++shape) {
    if (handleRect(*(*shape), HandlePosition::TopLeft).contains(point)) {
      return shape->get();
    }
  }
  return nullptr;
}

std::optional<HandlePosition> QDrawWidget::handleAt(const QPointF& point) const {
  if (selectedShape_ == nullptr) {
    return std::nullopt;
  }

  for (const SizeHandle& handle : selectedShape_->handles()) {
    if (handleRect(*selectedShape_, handle.position()).contains(point)) {
      return handle.position();
    }
  }
  return std::nullopt;
}

QRectF QDrawWidget::handleRect(const ::quickshot::Shape& shape, HandlePosition position) const {
  const qreal imageHandleSize = handleSize / zoomFactor_;
  const QPointF center = shape.handleCenter(SizeHandle{position});
  const qreal halfSize = imageHandleSize / 2.0;
  return {center.x() - halfSize, center.y() - halfSize, imageHandleSize, imageHandleSize};
}

QRectF QDrawWidget::constrainedMove(const QRectF& bounds, const QPointF& offset) const {
  const QRectF limits = imageBounds();
  const qreal horizontalOffset =
      std::clamp(offset.x(), limits.left() - bounds.left(), limits.right() - bounds.right());
  const qreal verticalOffset =
      std::clamp(offset.y(), limits.top() - bounds.top(), limits.bottom() - bounds.bottom());
  return bounds.translated(horizontalOffset, verticalOffset);
}

QRectF QDrawWidget::resizedBounds(const QRectF& bounds, HandlePosition handle,
                                  const QPointF& point) const {
  const QRectF limits = imageBounds();
  qreal left = bounds.left();
  qreal top = bounds.top();
  qreal right = bounds.right();
  qreal bottom = bounds.bottom();

  switch (handle) {
  case HandlePosition::TopLeft:
  case HandlePosition::Left:
  case HandlePosition::BottomLeft:
    left = std::clamp(point.x(), limits.left(), right - minimumShapeSize);
    break;
  case HandlePosition::TopRight:
  case HandlePosition::Right:
  case HandlePosition::BottomRight:
    right = std::clamp(point.x(), left + minimumShapeSize, limits.right());
    break;
  case HandlePosition::Top:
  case HandlePosition::Bottom:
    break;
  }

  switch (handle) {
  case HandlePosition::TopLeft:
  case HandlePosition::Top:
  case HandlePosition::TopRight:
    top = std::clamp(point.y(), limits.top(), bottom - minimumShapeSize);
    break;
  case HandlePosition::BottomRight:
  case HandlePosition::Bottom:
  case HandlePosition::BottomLeft:
    bottom = std::clamp(point.y(), top + minimumShapeSize, limits.bottom());
    break;
  case HandlePosition::Right:
  case HandlePosition::Left:
    break;
  }

  return {QPointF{left, top}, QPointF{right, bottom}};
}

void QDrawWidget::updateHoverCursor(const QPointF& point) {
  if (dragMode_ == DragMode::Rotate || (rotationMode_ && shapeAtRotationHandle(point) != nullptr)) {
    viewport()->setCursor(rotationCursor());
    return;
  }

  if (const std::optional<HandlePosition> handle = handleAt(point); handle.has_value()) {
    viewport()->setCursor(SizeHandle{*handle}.cursorShape());
    return;
  }

  if (shapeAt(point) != nullptr ||
      (creationMode_ != CreationMode::None && imageBounds().contains(point))) {
    viewport()->setCursor(Qt::CrossCursor);
    return;
  }

  viewport()->setCursor(Qt::ArrowCursor);
}

void QDrawWidget::updateDraggedShape(const QPointF& point) {
  if (selectedShape_ == nullptr) {
    return;
  }

  if (dragMode_ == DragMode::Create) {
    const QPointF boundedPoint{std::clamp(point.x(), imageBounds().left(), imageBounds().right()),
                               std::clamp(point.y(), imageBounds().top(), imageBounds().bottom())};
    selectedShape_->setBoundingRect(QRectF{dragStart_, boundedPoint}.normalized());
    return;
  }

  if (dragMode_ == DragMode::Move) {
    selectedShape_->setBoundingRect(constrainedMove(dragStartBounds_, point - dragStart_));
    return;
  }

  if (dragMode_ == DragMode::Resize && activeHandle_.has_value()) {
    const QPointF localPoint = dragStartImageToShape_.map(point);
    QRectF bounds = resizedBounds(dragStartBounds_, *activeHandle_, localPoint);
    const HandlePosition anchorHandle = oppositeHandle(*activeHandle_);
    const QPointF localAnchor = SizeHandle{anchorHandle}.center(bounds);
    const QPointF mappedAnchor = rotationTransform(bounds, dragStartRotation_).map(localAnchor);
    // A resized frame has a new center; translate it so rotation does not move the opposite handle.
    bounds.translate(resizeAnchorImage_ - mappedAnchor);
    selectedShape_->setBoundingRect(bounds);
    return;
  }

  if (dragMode_ == DragMode::Rotate) {
    const qreal currentMouseAngle = mouseAngle(selectedShape_->boundingRect().center(), point);
    selectedShape_->setRotationDegrees(dragStartRotation_ + currentMouseAngle -
                                       dragStartMouseAngle_);
  }
}

void QDrawWidget::drawSelectionHandles(QPainter& painter) const {
  if (selectedShape_ == nullptr || dragMode_ == DragMode::Rotate) {
    return;
  }

  QPen handlePen{Qt::white};
  handlePen.setCosmetic(true);
  painter.save();
  painter.setPen(handlePen);
  painter.setBrush(QBrush{Qt::white});
  for (const SizeHandle& handle : selectedShape_->handles()) {
    if (rotationMode_ && handle.position() == HandlePosition::TopLeft) {
      continue;
    }
    if (dragMode_ == DragMode::Resize && activeHandle_.has_value() &&
        handle.position() != *activeHandle_) {
      continue;
    }
    painter.drawRect(handleRect(*selectedShape_, handle.position()));
  }
  painter.restore();
}

void QDrawWidget::drawRotationHandles(QPainter& painter) const {
  if (!rotationMode_ && dragMode_ != DragMode::Rotate) {
    return;
  }

  QPen handlePen{rotationHandleColor};
  handlePen.setCosmetic(true);
  painter.save();
  painter.setPen(handlePen);
  painter.setBrush(QBrush{rotationHandleColor});
  for (const std::unique_ptr<::quickshot::Shape>& shape : shapes_) {
    if (dragMode_ == DragMode::Rotate && shape.get() != selectedShape_) {
      continue;
    }
    painter.drawRect(handleRect(*shape, HandlePosition::TopLeft));
  }
  painter.restore();
}

void QDrawWidget::setRotationMode(bool enabled) {
  if (rotationMode_ == enabled) {
    return;
  }

  rotationMode_ = enabled;
  const QPoint viewportPosition = viewport()->mapFromGlobal(QCursor::pos());
  if (viewport()->rect().contains(viewportPosition)) {
    updateHoverCursor(imagePosition(viewportPosition));
  } else if (!enabled && dragMode_ != DragMode::Rotate) {
    viewport()->setCursor(Qt::ArrowCursor);
  }
  viewport()->update();
}

void QDrawWidget::cloneShape(const ::quickshot::Shape& shape) {
  const QRectF sourceBounds = shape.boundingRect();
  constexpr std::array offsets = {QPointF{-cloneOffset, 0.0}, QPointF{cloneOffset, 0.0},
                                  QPointF{0.0, -cloneOffset}, QPointF{0.0, cloneOffset}};

  for (const QPointF& offset : offsets) {
    const QRectF cloneBounds = constrainedMove(sourceBounds, offset);
    if (cloneBounds == sourceBounds) {
      continue;
    }

    std::unique_ptr<::quickshot::Shape> clonedShape = shape.clone();
    clonedShape->setBoundingRect(cloneBounds);
    selectedShape_ = clonedShape.get();
    shapes_.push_back(std::move(clonedShape));
    viewport()->update();
    return;
  }
}

void QDrawWidget::deleteShape(const ::quickshot::Shape& shape) {
  const auto shapeToDelete =
      std::ranges::find_if(shapes_, [&shape](const std::unique_ptr<::quickshot::Shape>& candidate) {
        return candidate.get() == &shape;
      });
  if (shapeToDelete == shapes_.end()) {
    return;
  }

  if (selectedShape_ == shapeToDelete->get()) {
    selectedShape_ = nullptr;
  }
  shapes_.erase(shapeToDelete);
  activeHandle_.reset();
  dragMode_ = DragMode::None;
  viewport()->update();
}

void QDrawWidget::deleteAllShapes() {
  shapes_.clear();
  selectedShape_ = nullptr;
  activeHandle_.reset();
  dragMode_ = DragMode::None;
  viewport()->update();
}

void QDrawWidget::saveRois(const std::vector<const ::quickshot::Shape*>& targets) {
  if (targets.empty()) {
    return;
  }

  const QString selectedFileName = QFileDialog::getSaveFileName(
      this, tr("Save ROI"), QDir{lastSaveDirectory_}.filePath(QStringLiteral("roi.png")),
      tr("PNG Images (*.png)"));
  if (selectedFileName.isEmpty()) {
    return;
  }

  const QString baseFileName = pngFileName(selectedFileName);
  lastSaveDirectory_ = QFileInfo{baseFileName}.absolutePath();
  QSettings settings;
  settings.setValue(QStringLiteral("roi/lastSaveDirectory"), lastSaveDirectory_);

  std::vector<QString> outputFileNames;
  outputFileNames.reserve(targets.size());
  for (std::size_t index = 0; index < targets.size(); ++index) {
    outputFileNames.push_back(targets.size() == 1 ? baseFileName
                                                  : numberedPngFileName(baseFileName, index));
  }

  const bool outputExists = std::ranges::any_of(
      outputFileNames, [](const QString& fileName) { return QFileInfo::exists(fileName); });
  if (outputExists &&
      QMessageBox::question(this, tr("Overwrite ROI Files"),
                            tr("One or more ROI files already exist. Overwrite them?")) !=
          QMessageBox::Yes) {
    return;
  }

  for (std::size_t index = 0; index < targets.size(); ++index) {
    QString errorMessage;
    if (!saveRoiPng(image_, *targets[index], outputFileNames[index], &errorMessage)) {
      QMessageBox::warning(
          this, tr("Unable to Save ROI"),
          tr("Could not save %1: %2")
              .arg(QDir::toNativeSeparators(outputFileNames[index]), errorMessage));
      return;
    }
  }
}

void QDrawWidget::setCreationMode(CreationMode mode, bool enabled) {
  creationMode_ = enabled ? mode : CreationMode::None;
  if (!enabled) {
    viewport()->setCursor(Qt::ArrowCursor);
  }
}

void QDrawWidget::rotateImage(qreal degrees) {
  if (image_.isNull()) {
    return;
  }

  QTransform rotation;
  rotation.rotate(degrees);
  const QTransform shapeTransformation =
      QImage::trueMatrix(rotation, image_.width(), image_.height());
  image_ = image_.transformed(rotation);
  for (const std::unique_ptr<::quickshot::Shape>& shape : shapes_) {
    shape->transform(shapeTransformation);
  }
  updateScrollBars();
  viewport()->update();
}

QSize QDrawWidget::scaledImageSize() const {
  if (image_.isNull()) {
    return {};
  }

  return {std::max(1, qRound(static_cast<qreal>(image_.width()) * zoomFactor_)),
          std::max(1, qRound(static_cast<qreal>(image_.height()) * zoomFactor_))};
}

void QDrawWidget::updateScrollBars() {
  const QSize pageSize = viewport()->size();
  const QSize imageSize = scaledImageSize();
  const int horizontalMaximum = std::max(0, imageSize.width() - pageSize.width());
  const int verticalMaximum = std::max(0, imageSize.height() - pageSize.height());

  // The page step is the visible extent; a zero maximum lets Qt hide an unnecessary scrollbar.
  horizontalScrollBar()->setPageStep(pageSize.width());
  horizontalScrollBar()->setRange(0, horizontalMaximum);

  verticalScrollBar()->setPageStep(pageSize.height());
  verticalScrollBar()->setRange(0, verticalMaximum);
}

} // namespace quickshot
