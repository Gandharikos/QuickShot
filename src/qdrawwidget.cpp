#include "quickshot/qdrawwidget.hpp"

#include "quickshot/batch_save_dialog.hpp"
#include "quickshot/commands/shape_commands.hpp"
#include "quickshot/roi_exporter.hpp"
#include "quickshot/shapes/shape.hpp"

#include <QAction>
#include <QBrush>
#include <QColor>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QImageReader>
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
#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <ranges>
#include <utility>

namespace quickshot {
namespace {

constexpr qreal minimumZoom = 0.1;
constexpr qreal maximumZoom = 8.0;
constexpr qreal zoomPerWheelStep = 1.1;
constexpr qreal wheelStepAngle = 120.0;
constexpr qreal handleSize = 8.0;
constexpr qreal cloneOffset = 10.0;

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

const DragState& creationState(const Shape& shape) {
  switch (shape.creationKind()) {
  case CreationKind::Drag:
    return CreateState::instance();
  case CreationKind::MultiPoint:
    return MultiPointCreateState::instance();
  }
  return CreateState::instance();
}

} // namespace

class QDrawWidget::ImageDocument final {
public:
  ImageDocument(QString filePath, QImage image)
      : filePath(std::move(filePath)), image(std::move(image)) {}

  QString filePath;
  QImage image;
  std::vector<std::unique_ptr<::quickshot::Shape>> shapes;
  ::quickshot::Shape* selectedShape = nullptr;
  QUndoStack undoStack;
};

QDrawWidget::QDrawWidget(QWidget* parent)
    : QAbstractScrollArea(parent), undoGroup_(this), fallbackUndoStack_(this) {
  undoGroup_.addStack(&fallbackUndoStack_);
  undoGroup_.setActiveStack(&fallbackUndoStack_);
  setFrameShape(QFrame::NoFrame);
  viewport()->setAutoFillBackground(false);
  viewport()->setMouseTracking(true);
  initializeContextActions();

  QSettings settings;
  lastSaveDirectory_ = settings.value(QStringLiteral("roi/lastSaveDirectory")).toString();
  if (lastSaveDirectory_.isEmpty()) {
    lastSaveDirectory_ = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
  }
  if (lastSaveDirectory_.isEmpty()) {
    lastSaveDirectory_ = QDir::homePath();
  }
}

QDrawWidget::~QDrawWidget() = default;

bool QDrawWidget::loadImage(const QString& fileName) { return loadImages({fileName}).isEmpty(); }

QStringList QDrawWidget::loadImages(const QStringList& fileNames) {
  QStringList rejectedFiles;
  std::vector<std::unique_ptr<ImageDocument>> loadedDocuments;
  loadedDocuments.reserve(static_cast<std::size_t>(fileNames.size()));
  for (const QString& fileName : fileNames) {
    QImageReader reader{fileName};
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull()) {
      rejectedFiles.push_back(fileName);
      continue;
    }
    loadedDocuments.push_back(
        std::make_unique<ImageDocument>(QFileInfo{fileName}.absoluteFilePath(), std::move(image)));
  }

  if (loadedDocuments.empty()) {
    return rejectedFiles;
  }

  cancelDrag();
  undoGroup_.setActiveStack(&fallbackUndoStack_);
  fallbackUndoStack_.clear();
  image_ = {};
  selectedShape_ = nullptr;
  documents_.clear();
  shapes_.clear();
  documents_ = std::move(loadedDocuments);
  for (const std::unique_ptr<ImageDocument>& document : documents_) {
    undoGroup_.addStack(&document->undoStack);
  }
  currentImageIndex_ = -1;
  setCurrentImageIndex(0);
  emit imageCollectionChanged();
  emit imageAvailabilityChanged(true);
  return rejectedFiles;
}

bool QDrawWidget::hasImage() const noexcept { return !image_.isNull(); }

qsizetype QDrawWidget::imageCount() const noexcept {
  return static_cast<qsizetype>(documents_.size());
}

qsizetype QDrawWidget::currentImageIndex() const noexcept { return currentImageIndex_; }

QString QDrawWidget::imagePathAt(qsizetype index) const {
  if (index < 0 || index >= imageCount()) {
    return {};
  }
  return documents_[static_cast<std::size_t>(index)]->filePath;
}

QImage QDrawWidget::thumbnailAt(qsizetype index, const QSize& size) const {
  if (index < 0 || index >= imageCount()) {
    return {};
  }
  return documentImage(index).scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

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

QUndoStack& QDrawWidget::undoStack() noexcept {
  Q_ASSERT(undoGroup_.activeStack() != nullptr);
  return *undoGroup_.activeStack();
}

const QUndoStack& QDrawWidget::undoStack() const noexcept {
  Q_ASSERT(undoGroup_.activeStack() != nullptr);
  return *undoGroup_.activeStack();
}

QUndoGroup& QDrawWidget::undoGroup() noexcept { return undoGroup_; }

const QUndoGroup& QDrawWidget::undoGroup() const noexcept { return undoGroup_; }

void QDrawWidget::setCurrentImageIndex(qsizetype index) {
  if (index < 0 || index >= imageCount() || index == currentImageIndex_) {
    return;
  }

  cancelDrag();
  storeCurrentDocument();
  ImageDocument& document = *documents_[static_cast<std::size_t>(index)];
  image_ = document.image;
  shapes_ = std::move(document.shapes);
  selectedShape_ = document.selectedShape;
  document.selectedShape = nullptr;
  currentImageIndex_ = index;
  undoGroup_.setActiveStack(&document.undoStack);

  const qreal previousZoom = zoomFactor_;
  zoomFactor_ = 1.0;
  horizontalScrollBar()->setValue(0);
  verticalScrollBar()->setValue(0);
  updateScrollBars();
  viewport()->update();
  emit cursorLeftImage();
  if (!qFuzzyCompare(previousZoom, zoomFactor_)) {
    emit zoomFactorChanged(zoomFactor_);
  }
  emit currentImageChanged(currentImageIndex_);
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

void QDrawWidget::setCreationMode(ShapeType type, bool enabled) {
  if (enabled) {
    if (creationType_ != type) {
      cancelDrag();
    }
    creationType_ = type;
  } else if (creationType_ == type) {
    cancelDrag();
    creationType_.reset();
    viewport()->setCursor(Qt::ArrowCursor);
  }
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
    batchSaveRoiAction_->setEnabled(imageCount() > 1);
    menu.addActions({saveRoiAction_, batchSaveRoiAction_});
    menu.addSeparator();
    menu.addActions({cloneShapeAction_, deleteShapeAction_});
  } else {
    const bool hasShapes = !shapes_.empty();
    saveAllRoisAction_->setEnabled(hasShapes);
    batchSaveAllRoisAction_->setEnabled(hasShapes && imageCount() > 1);
    deleteAllShapesAction_->setEnabled(hasShapes);
    menu.addActions({saveAllRoisAction_, batchSaveAllRoisAction_});
    menu.addSeparator();
    menu.addAction(deleteAllShapesAction_);
  }

  menu.exec(event->globalPos());
  event->accept();
}

void QDrawWidget::initializeContextActions() {
  saveRoiAction_ = new QAction{tr("Save ROI"), this};
  saveRoiAction_->setObjectName("saveRoiAction");
  connect(saveRoiAction_, &QAction::triggered, this, &QDrawWidget::saveSelectedRoi);

  batchSaveRoiAction_ = new QAction{tr("Batch Save"), this};
  batchSaveRoiAction_->setObjectName("batchSaveRoiAction");
  connect(batchSaveRoiAction_, &QAction::triggered, this, &QDrawWidget::batchSaveSelectedRoi);

  cloneShapeAction_ = new QAction{tr("Clone"), this};
  cloneShapeAction_->setObjectName("cloneShapeAction");
  connect(cloneShapeAction_, &QAction::triggered, this, &QDrawWidget::cloneSelectedShape);

  deleteShapeAction_ = new QAction{tr("Delete"), this};
  deleteShapeAction_->setObjectName("deleteShapeAction");
  connect(deleteShapeAction_, &QAction::triggered, this, &QDrawWidget::deleteSelectedShape);

  saveAllRoisAction_ = new QAction{tr("Save All"), this};
  saveAllRoisAction_->setObjectName("saveAllRoisAction");
  connect(saveAllRoisAction_, &QAction::triggered, this, &QDrawWidget::saveAllRois);

  batchSaveAllRoisAction_ = new QAction{tr("Batch Save All"), this};
  batchSaveAllRoisAction_->setObjectName("batchSaveAllRoisAction");
  connect(batchSaveAllRoisAction_, &QAction::triggered, this, &QDrawWidget::batchSaveAllRois);

  deleteAllShapesAction_ = new QAction{tr("Delete All"), this};
  deleteAllShapesAction_->setObjectName("deleteAllShapesAction");
  connect(deleteAllShapesAction_, &QAction::triggered, this, &QDrawWidget::deleteAllShapes);
}

void QDrawWidget::saveSelectedRoi() {
  if (selectedShape_ != nullptr) {
    saveRois({selectedShape_});
  }
}

void QDrawWidget::batchSaveSelectedRoi() {
  if (selectedShape_ != nullptr) {
    batchSaveRois({selectedShape_});
  }
}

void QDrawWidget::cloneSelectedShape() {
  if (selectedShape_ != nullptr) {
    pushCommand(std::make_unique<CloneShapeCommand>(*this, *selectedShape_));
  }
}

void QDrawWidget::deleteSelectedShape() {
  if (selectedShape_ != nullptr) {
    pushCommand(std::make_unique<DeleteShapeCommand>(*this, *selectedShape_));
  }
}

void QDrawWidget::saveAllRois() {
  std::vector<const ::quickshot::Shape*> targets;
  targets.reserve(shapes_.size());
  for (const std::unique_ptr<::quickshot::Shape>& shape : shapes_) {
    targets.push_back(shape.get());
  }
  saveRois(targets);
}

void QDrawWidget::batchSaveAllRois() {
  std::vector<const ::quickshot::Shape*> targets;
  targets.reserve(shapes_.size());
  for (const std::unique_ptr<::quickshot::Shape>& shape : shapes_) {
    targets.push_back(shape.get());
  }
  batchSaveRois(targets);
}

void QDrawWidget::deleteAllShapes() {
  pushCommand(std::make_unique<DeleteAllShapesCommand>(*this));
}

void QDrawWidget::leaveEvent(QEvent* event) {
  emit cursorLeftImage();
  QAbstractScrollArea::leaveEvent(event);
}

void QDrawWidget::mouseMoveEvent(QMouseEvent* event) {
  const QPointF point = imagePosition(event->position());
  if (!image_.isNull() && point.x() >= 0.0 && point.y() >= 0.0 &&
      point.x() < static_cast<qreal>(image_.width()) &&
      point.y() < static_cast<qreal>(image_.height())) {
    emit cursorImagePositionChanged(point);
  } else {
    emit cursorLeftImage();
  }

  if (dragController_.isActive()) {
    dragController_.update(point);
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
  if (dragController_.isActive()) {
    const DragProgress progress = dragController_.press(event->button(), point);
    if (progress == DragProgress::Finish) {
      if (event->button() == Qt::RightButton) {
        suppressContextMenu_ = true;
      }
      std::optional<DragCompletion> completion = dragController_.finish();
      if (completion.has_value()) {
        completeDrag(std::move(*completion));
      }
    }
    if (progress != DragProgress::Ignore) {
      viewport()->update();
      event->accept();
      return;
    }

    QAbstractScrollArea::mousePressEvent(event);
    return;
  }

  if (event->button() == Qt::RightButton) {
    if (const std::optional<ShapeHandle> handle = handleAt(point); handle.has_value()) {
      dragController_.begin(RotateState::instance(), {.shape = *selectedShape_,
                                                      .origin = point,
                                                      .imageBounds = imageBounds(),
                                                      .handle = handle,
                                                      .previousSelection = selectedShape_});
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

  if (const std::optional<ShapeHandle> handle = handleAt(point); handle.has_value()) {
    dragController_.begin(ResizeState::instance(), {.shape = *selectedShape_,
                                                    .origin = point,
                                                    .imageBounds = imageBounds(),
                                                    .handle = handle,
                                                    .previousSelection = selectedShape_});
    viewport()->update();
    event->accept();
    return;
  }

  if (::quickshot::Shape* hitShape = shapeAt(point); hitShape != nullptr) {
    ::quickshot::Shape* previousSelection = selectedShape_;
    selectedShape_ = hitShape;
    dragController_.begin(MoveState::instance(), {.shape = *selectedShape_,
                                                  .origin = point,
                                                  .imageBounds = imageBounds(),
                                                  .handle = std::nullopt,
                                                  .previousSelection = previousSelection});
    viewport()->update();
    event->accept();
    return;
  }

  if (creationType_.has_value()) {
    const ShapeType creationType = *creationType_;
    ::quickshot::Shape* previousSelection = selectedShape_;
    const QRectF initialBounds{point, point};
    std::unique_ptr<::quickshot::Shape> shape =
        ::quickshot::Shape::make(creationType, initialBounds);

    selectedShape_ = shape.get();
    shapes_.push_back(std::move(shape));
    dragController_.begin(creationState(*selectedShape_), {.shape = *selectedShape_,
                                                           .origin = point,
                                                           .imageBounds = imageBounds(),
                                                           .handle = std::nullopt,
                                                           .previousSelection = previousSelection});
    viewport()->update();
    event->accept();
    return;
  }

  selectedShape_ = nullptr;
  viewport()->update();
  event->accept();
}

void QDrawWidget::mouseReleaseEvent(QMouseEvent* event) {
  if (!dragController_.isActive()) {
    QAbstractScrollArea::mouseReleaseEvent(event);
    return;
  }

  const DragProgress progress =
      dragController_.release(event->button(), imagePosition(event->position()));
  if (progress == DragProgress::Finish) {
    std::optional<DragCompletion> completion = dragController_.finish();
    if (completion.has_value()) {
      completeDrag(std::move(*completion));
    }
  } else if (progress == DragProgress::Ignore) {
    QAbstractScrollArea::mouseReleaseEvent(event);
    return;
  }

  updateHoverCursor(imagePosition(event->position()));
  viewport()->update();
  event->accept();
}

void QDrawWidget::paintEvent(QPaintEvent* event) {
  QAbstractScrollArea::paintEvent(event);

  QPainter painter(viewport());
  const QRectF viewportRect{0.0, 0.0, static_cast<qreal>(viewport()->width()),
                            static_cast<qreal>(viewport()->height())};
  // Uncovered viewport pixels use the application's default light canvas color.
  painter.fillRect(viewportRect, Qt::white);

  if (image_.isNull()) {
    return;
  }

  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  // Scrollbar values are offsets in scaled content coordinates, so translate in
  // the opposite direction before scaling the image uniformly.
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
}

void QDrawWidget::resizeEvent(QResizeEvent* event) {
  QAbstractScrollArea::resizeEvent(event);
  // A resized viewport changes the visible page and therefore the valid scroll
  // range.
  updateScrollBars();
}

void QDrawWidget::wheelEvent(QWheelEvent* event) {
  // y() is vertical wheel rotation; x() carries horizontal input from tilt
  // wheels or touchpads.
  const int angleDelta = event->angleDelta().y();
  if (image_.isNull() || !event->modifiers().testFlag(Qt::ControlModifier) || angleDelta == 0) {
    QAbstractScrollArea::wheelEvent(event);
    return;
  }

  const qreal previousZoom = zoomFactor_;
  // Qt reports angleDelta in eighths of a degree, so a typical 15-degree notch
  // is 120 units; keeping a fractional step also supports high-resolution
  // wheels.
  const qreal wheelSteps = static_cast<qreal>(angleDelta) / wheelStepAngle;
  // Multiplication gives every notch the same relative visual change, while
  // clamp keeps the resulting scale within the supported 10%-800% range.
  setZoomFactor(previousZoom * std::pow(zoomPerWheelStep, wheelSteps));

  // Compare floating-point values approximately; clamp can leave the scale
  // unchanged at a limit.
  if (qFuzzyCompare(zoomFactor_, previousZoom)) {
    event->accept();
    return;
  }

  // Keeping the scrollbar offsets unchanged makes the image's top-left corner
  // the zoom anchor.
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
  for (const auto& shape : std::views::reverse(shapes_)) {
    if (shape->contains(point)) {
      return shape.get();
    }
  }
  return nullptr;
}

std::optional<ShapeHandle> QDrawWidget::handleAt(const QPointF& point) const {
  if (selectedShape_ == nullptr) {
    return std::nullopt;
  }

  for (const ShapeHandle& handle : selectedShape_->handles()) {
    if (handleRect(*selectedShape_, handle).contains(point)) {
      return handle;
    }
  }
  return std::nullopt;
}

QRectF QDrawWidget::handleRect(const ::quickshot::Shape& shape, const ShapeHandle& handle) const {
  // Convert the fixed viewport handle size to image coordinates so zooming does
  // not change its on-screen size.
  const qreal imageHandleSize = handleSize / zoomFactor_;
  // Use the transformed center because rotated shape handles no longer lie on
  // the axis-aligned bounding rectangle.
  const QPointF center = shape.handleCenter(handle);
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

void QDrawWidget::updateHoverCursor(const QPointF& point) {
  if (const std::optional<ShapeHandle> handle = handleAt(point); handle.has_value()) {
    viewport()->setCursor(handle->cursorShape());
    return;
  }

  if (shapeAt(point) != nullptr || (creationType_.has_value() && imageBounds().contains(point))) {
    viewport()->setCursor(Qt::CrossCursor);
    return;
  }

  viewport()->setCursor(Qt::ArrowCursor);
}

void QDrawWidget::drawSelectionHandles(QPainter& painter) const {
  if (selectedShape_ == nullptr) {
    return;
  }

  const QColor handleColor =
      selectedShape_->isCreationComplete() ? QColor{Qt::white} : QColor{0, 200, 83};
  QPen handlePen{handleColor};
  handlePen.setCosmetic(true);
  painter.save();
  painter.setPen(handlePen);
  painter.setBrush(QBrush{handleColor});
  const std::optional<ShapeHandle> activeHandle = dragController_.activeHandle();
  for (const ShapeHandle& handle : selectedShape_->handles()) {
    if (activeHandle.has_value() && handle != *activeHandle) {
      continue;
    }
    painter.drawRect(handleRect(*selectedShape_, handle));
  }
  painter.restore();
}

std::unique_ptr<::quickshot::Shape>
QDrawWidget::makeOffsetClone(const ::quickshot::Shape& shape) const {
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
    return clonedShape;
  }

  return nullptr;
}

void QDrawWidget::pushCommand(std::unique_ptr<QUndoCommand> command) {
  undoStack().push(command.release());
}

// Commit a finished gesture as one undoable transaction, or discard an invalid creation.
void QDrawWidget::completeDrag(DragCompletion completion) {
  const auto position = std::ranges::find_if(
      shapes_, [&completion](const std::unique_ptr<::quickshot::Shape>& shape) {
        return shape.get() == completion.shape;
      });
  if (position == shapes_.end()) {
    return;
  }

  if (completion.result == DragResult::RemoveShape) {
    shapes_.erase(position);
    selectedShape_ = completion.previousSelection;
    return;
  }

  if (completion.createsShape) {
    const auto insertionIndex = static_cast<std::size_t>(std::distance(shapes_.begin(), position));
    std::unique_ptr<::quickshot::Shape> shape = std::move(*position);
    shapes_.erase(position);
    selectedShape_ = completion.previousSelection;
    pushCommand(std::make_unique<CreateShapeCommand>(*this, std::move(shape), insertionIndex,
                                                     completion.previousSelection));
    return;
  }

  if (completion.before == nullptr || completion.after == nullptr ||
      completion.before->equals(*completion.after)) {
    return;
  }

  pushCommand(std::make_unique<TransformShapeCommand>(
      *this, *completion.shape, std::move(completion.before), std::move(completion.after),
      completion.undoText));
}

// Roll back the live drag preview before another command mutates the same shape model.
void QDrawWidget::cancelDrag() {
  const std::optional<DragCompletion> cancellation = dragController_.cancel();
  if (!cancellation.has_value()) {
    return;
  }

  if (cancellation->result == DragResult::RemoveShape) {
    const auto position = std::ranges::find_if(
        shapes_, [&cancellation](const std::unique_ptr<::quickshot::Shape>& shape) {
          return shape.get() == cancellation->shape;
        });
    if (position != shapes_.end()) {
      shapes_.erase(position);
    }
  }
  selectedShape_ = cancellation->previousSelection;
  viewport()->update();
}

void QDrawWidget::clearUndoHistoryForUntrackedEdit() {
  cancelDrag();
  // Image rotation changes both the bitmap and every shape's coordinate system;
  // keep it outside the shape-only undo timeline for now.
  undoStack().clear();
}

void QDrawWidget::storeCurrentDocument() {
  if (currentImageIndex_ < 0 || currentImageIndex_ >= imageCount()) {
    return;
  }

  ImageDocument& document = *documents_[static_cast<std::size_t>(currentImageIndex_)];
  document.image = image_;
  document.shapes = std::move(shapes_);
  document.selectedShape = selectedShape_;
  selectedShape_ = nullptr;
}

const QImage& QDrawWidget::documentImage(qsizetype index) const {
  Q_ASSERT(index >= 0 && index < imageCount());
  if (index == currentImageIndex_) {
    return image_;
  }
  return documents_[static_cast<std::size_t>(index)]->image;
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

void QDrawWidget::batchSaveRois(const std::vector<const ::quickshot::Shape*>& targets) {
  if (targets.empty() || imageCount() < 2) {
    return;
  }

  std::vector<BatchSaveRow> rows;
  rows.reserve(static_cast<std::size_t>(imageCount()));
  for (qsizetype imageIndex = 0; imageIndex < imageCount(); ++imageIndex) {
    const QImage& targetImage = documentImage(imageIndex);
    const bool savable =
        std::ranges::all_of(targets, [&targetImage](const ::quickshot::Shape* shape) {
          return shape != nullptr && isRoiWithinImage(targetImage, *shape);
        });
    rows.push_back({.imagePath = imagePathAt(imageIndex),
                    .savable = savable,
                    .statusMessage = savable ? tr("All selected ROIs fit inside this image.")
                                             : tr("One or more ROI bounds exceed this image.")});
  }

  BatchSaveDialog dialog{rows, lastSaveDirectory_, this};
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  lastSaveDirectory_ = QDir{dialog.outputDirectory()}.absolutePath();
  QSettings{}.setValue(QStringLiteral("roi/lastSaveDirectory"), lastSaveDirectory_);

  struct BatchOutput {
    qsizetype imageIndex;
    const ::quickshot::Shape* shape;
    QString fileName;
  };

  std::vector<BatchOutput> outputs;
  const QString timestamp =
      QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
  std::size_t outputSequence = 0;
  for (qsizetype imageIndex = 0; imageIndex < imageCount(); ++imageIndex) {
    if (!rows[static_cast<std::size_t>(imageIndex)].savable) {
      continue;
    }

    const QString stem = QFileInfo{imagePathAt(imageIndex)}.completeBaseName();
    for (std::size_t shapeIndex = 0; shapeIndex < targets.size(); ++shapeIndex) {
      ++outputSequence;
      const QString suffix = QStringLiteral("_roi_%1_%2.png")
                                 .arg(timestamp)
                                 .arg(static_cast<qulonglong>(outputSequence), 3, 10, QChar{'0'});
      outputs.push_back({.imageIndex = imageIndex,
                         .shape = targets[shapeIndex],
                         .fileName = QDir{lastSaveDirectory_}.filePath(stem + suffix)});
    }
  }

  const bool outputExists = std::ranges::any_of(
      outputs, [](const BatchOutput& output) { return QFileInfo::exists(output.fileName); });
  if (outputExists &&
      QMessageBox::question(this, tr("Overwrite ROI Files"),
                            tr("One or more ROI files already exist. Overwrite them?")) !=
          QMessageBox::Yes) {
    return;
  }

  QStringList failures;
  for (const BatchOutput& output : outputs) {
    QString errorMessage;
    if (!saveRoiPng(documentImage(output.imageIndex), *output.shape, output.fileName,
                    &errorMessage)) {
      failures.push_back(tr("%1: %2").arg(output.fileName, errorMessage));
    }
  }
  if (!failures.isEmpty()) {
    QMessageBox::warning(this, tr("Some ROI Files Could Not Be Saved"), failures.join('\n'));
  }
}

void QDrawWidget::rotateImage(qreal degrees) {
  if (image_.isNull()) {
    return;
  }

  clearUndoHistoryForUntrackedEdit();

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
  emit imageThumbnailChanged(currentImageIndex_);
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

  // The page step is the visible extent; a zero maximum lets Qt hide an
  // unnecessary scrollbar.
  horizontalScrollBar()->setPageStep(pageSize.width());
  horizontalScrollBar()->setRange(0, horizontalMaximum);

  verticalScrollBar()->setPageStep(pageSize.height());
  verticalScrollBar()->setRange(0, verticalMaximum);
}

} // namespace quickshot
