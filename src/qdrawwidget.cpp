#include "quickshot/qdrawwidget.hpp"

#include "quickshot/drag_state.hpp"
#include "quickshot/roi_exporter.hpp"
#include "quickshot/shape.hpp"

#include <QAction>
#include <QBrush>
#include <QColor>
#include <QContextMenuEvent>
#include <QCursor>
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

} // namespace

QDrawWidget::QDrawWidget(QWidget* parent) : QAbstractScrollArea(parent) {
  setFrameShape(QFrame::NoFrame);
  viewport()->setAutoFillBackground(false);
  viewport()->setMouseTracking(true);

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

bool QDrawWidget::loadImage(const QString& fileName) {
  QImageReader reader(fileName);
  reader.setAutoTransform(true);

  QImage image = reader.read();
  if (image.isNull()) {
    return false;
  }

  image_ = std::move(image);
  dragState_.reset();
  shapes_.clear();
  selectedShape_ = nullptr;
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

void QDrawWidget::setCreationMode(ShapeType type, bool enabled) {
  if (enabled) {
    creationType_ = type;
  } else if (creationType_ == type) {
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

void QDrawWidget::mouseMoveEvent(QMouseEvent* event) {
  const QPointF point = imagePosition(event->position());
  if (dragState_ != nullptr) {
    dragState_->update(point);
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
  if (event->button() == Qt::RightButton) {
    if (const std::optional<HandlePosition> handle = handleAt(point); handle.has_value()) {
      dragState_ = std::make_unique<RotateState>(*selectedShape_, *handle, point);
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
    dragState_ = std::make_unique<ResizeState>(*selectedShape_, *handle, imageBounds());
    viewport()->update();
    event->accept();
    return;
  }

  if (::quickshot::Shape* hitShape = shapeAt(point); hitShape != nullptr) {
    selectedShape_ = hitShape;
    dragState_ = std::make_unique<MoveState>(*selectedShape_, point, imageBounds());
    viewport()->update();
    event->accept();
    return;
  }

  if (creationType_.has_value()) {
    const QRectF initialBounds{point, point};
    std::unique_ptr<::quickshot::Shape> shape =
        ::quickshot::Shape::make(*creationType_, initialBounds);

    selectedShape_ = shape.get();
    shapes_.push_back(std::move(shape));
    dragState_ = std::make_unique<CreateState>(*selectedShape_, point, imageBounds());
    viewport()->update();
    event->accept();
    return;
  }

  selectedShape_ = nullptr;
  viewport()->update();
  event->accept();
}

void QDrawWidget::mouseReleaseEvent(QMouseEvent* event) {
  if (dragState_ == nullptr || event->button() != dragState_->completionButton()) {
    QAbstractScrollArea::mouseReleaseEvent(event);
    return;
  }

  const DragResult result = dragState_->finish();
  dragState_.reset();
  if (result == DragResult::RemoveShape) {
    shapes_.pop_back();
    selectedShape_ = nullptr;
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

void QDrawWidget::updateHoverCursor(const QPointF& point) {
  if (const std::optional<HandlePosition> handle = handleAt(point); handle.has_value()) {
    viewport()->setCursor(SizeHandle{*handle}.cursorShape());
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

  QPen handlePen{Qt::white};
  handlePen.setCosmetic(true);
  painter.save();
  painter.setPen(handlePen);
  painter.setBrush(QBrush{Qt::white});
  const std::optional<HandlePosition> activeHandle =
      dragState_ != nullptr ? dragState_->activeHandle() : std::nullopt;
  for (const SizeHandle& handle : selectedShape_->handles()) {
    if (activeHandle.has_value() && handle.position() != *activeHandle) {
      continue;
    }
    painter.drawRect(handleRect(*selectedShape_, handle.position()));
  }
  painter.restore();
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
  dragState_.reset();
  shapes_.erase(shapeToDelete);
  viewport()->update();
}

void QDrawWidget::deleteAllShapes() {
  dragState_.reset();
  shapes_.clear();
  selectedShape_ = nullptr;
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

  // The page step is the visible extent; a zero maximum lets Qt hide an
  // unnecessary scrollbar.
  horizontalScrollBar()->setPageStep(pageSize.width());
  horizontalScrollBar()->setRange(0, horizontalMaximum);

  verticalScrollBar()->setPageStep(pageSize.height());
  verticalScrollBar()->setRange(0, verticalMaximum);
}

} // namespace quickshot
