#include "quickshot/canvas_view.hpp"

#include "quickshot/batch_save_dialog.hpp"
#include "quickshot/image_document.hpp"
#include "quickshot/image_scene.hpp"
#include "quickshot/roi_exporter.hpp"
#include "quickshot/shape_commands.hpp"

#include <QAction>
#include <QContextMenuEvent>
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
#include <QScrollBar>
#include <QSettings>
#include <QStandardPaths>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <utility>

namespace quickshot {
namespace {

constexpr qreal minimumZoom = 0.1;
constexpr qreal maximumZoom = 8.0;
constexpr qreal zoomPerWheelStep = 1.1;
constexpr qreal wheelStepAngle = 120.0;

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
  return baseFile.dir().filePath(
      QStringLiteral("%1_%2.png")
          .arg(baseFile.completeBaseName())
          .arg(static_cast<qulonglong>(index) + 1ULL, 3, 10, QChar{'0'}));
}

} // namespace

CanvasView::CanvasView(QWidget* parent)
    : QGraphicsView(parent), undoGroup_(this), fallbackUndoStack_(this) {
  undoGroup_.addStack(&fallbackUndoStack_);
  undoGroup_.setActiveStack(&fallbackUndoStack_);
  setAlignment(Qt::AlignLeft | Qt::AlignTop);
  setFrameShape(QFrame::NoFrame);
  setBackgroundBrush(Qt::white);
  setDragMode(QGraphicsView::NoDrag);
  setMouseTracking(true);
  setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
  setResizeAnchor(QGraphicsView::NoAnchor);
  setTransformationAnchor(QGraphicsView::NoAnchor);
  initializeContextActions();

  lastSaveDirectory_ = QSettings{}.value(QStringLiteral("roi/lastSaveDirectory")).toString();
  if (lastSaveDirectory_.isEmpty()) {
    lastSaveDirectory_ = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
  }
  if (lastSaveDirectory_.isEmpty()) {
    lastSaveDirectory_ = QDir::homePath();
  }
}

CanvasView::~CanvasView() {
  setScene(nullptr);
  undoGroup_.setActiveStack(&fallbackUndoStack_);
  for (const std::unique_ptr<ImageDocument>& document : documents_) {
    undoGroup_.removeStack(&document->undoStack());
  }
}

bool CanvasView::loadImage(const QString& fileName) { return loadImages({fileName}).isEmpty(); }

QStringList CanvasView::loadImages(const QStringList& fileNames) {
  QStringList rejectedFiles;
  std::vector<std::unique_ptr<ImageDocument>> loaded;
  for (const QString& fileName : fileNames) {
    QImageReader reader{fileName};
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull()) {
      rejectedFiles.push_back(fileName);
      continue;
    }
    loaded.push_back(
        std::make_unique<ImageDocument>(QFileInfo{fileName}.absoluteFilePath(), std::move(image)));
  }
  if (loaded.empty()) {
    return rejectedFiles;
  }

  const bool hadImages = hasImage();
  const qsizetype firstNewIndex = imageCount();
  documents_.reserve(documents_.size() + loaded.size());
  for (std::unique_ptr<ImageDocument>& document : loaded) {
    ImageDocument* loadedDocument = document.get();
    connect(&loadedDocument->scene(), &ImageScene::shapeCollectionChanged, this,
            [this, loadedDocument]() {
              if (currentDocument() == loadedDocument) {
                emit currentShapeAvailabilityChanged(shapeCount() > 0);
              }
            });
    undoGroup_.addStack(&document->undoStack());
    documents_.push_back(std::move(document));
  }
  setCurrentImageIndex(firstNewIndex);
  emit imageCollectionChanged();
  if (!hadImages) {
    emit imageAvailabilityChanged(true);
  }
  return rejectedFiles;
}

bool CanvasView::hasImage() const noexcept { return currentDocument() != nullptr; }

qsizetype CanvasView::imageCount() const noexcept {
  return static_cast<qsizetype>(documents_.size());
}

qsizetype CanvasView::currentImageIndex() const noexcept { return currentImageIndex_; }

QString CanvasView::imagePathAt(qsizetype index) const {
  return index >= 0 && index < imageCount()
             ? documents_[static_cast<std::size_t>(index)]->filePath()
             : QString{};
}

QImage CanvasView::thumbnailAt(qsizetype index, const QSize& size) const {
  return index >= 0 && index < imageCount()
             ? documents_[static_cast<std::size_t>(index)]->image().scaled(
                   size, Qt::KeepAspectRatio, Qt::SmoothTransformation)
             : QImage{};
}

qreal CanvasView::zoomFactor() const noexcept { return zoomFactor_; }

QSize CanvasView::sizeHint() const { return {640, 360}; }

qsizetype CanvasView::shapeCount() const noexcept {
  return imageScene() == nullptr ? 0 : imageScene()->shapeCount();
}

qsizetype CanvasView::shapeCountAt(qsizetype imageIndex) const noexcept {
  return imageIndex >= 0 && imageIndex < imageCount()
             ? documents_[static_cast<std::size_t>(imageIndex)]->scene().shapeCount()
             : 0;
}

const ::quickshot::Shape* CanvasView::shapeAt(qsizetype index) const {
  const ShapeItem* item = imageScene() == nullptr ? nullptr : imageScene()->shapeItemAt(index);
  return item == nullptr ? nullptr : &item->model();
}

QUndoStack& CanvasView::undoStack() noexcept {
  Q_ASSERT(undoGroup_.activeStack() != nullptr);
  return *undoGroup_.activeStack();
}

const QUndoStack& CanvasView::undoStack() const noexcept {
  Q_ASSERT(undoGroup_.activeStack() != nullptr);
  return *undoGroup_.activeStack();
}

QUndoGroup& CanvasView::undoGroup() noexcept { return undoGroup_; }

const QUndoGroup& CanvasView::undoGroup() const noexcept { return undoGroup_; }

void CanvasView::setCurrentImageIndex(qsizetype index) {
  if (index < 0 || index >= imageCount() || index == currentImageIndex_) {
    return;
  }
  if (ImageScene* current = imageScene(); current != nullptr) {
    current->cancelCreation();
  }
  currentImageIndex_ = index;
  ImageDocument& document = *documents_[static_cast<std::size_t>(index)];
  setScene(&document.scene());
  undoGroup_.setActiveStack(&document.undoStack());
  if (creationType_.has_value()) {
    document.scene().setCreationMode(*creationType_, true);
  }
  setZoomFactor(1.0);
  horizontalScrollBar()->setValue(0);
  verticalScrollBar()->setValue(0);
  emit cursorLeftImage();
  emit currentImageChanged(index);
  emit currentShapeAvailabilityChanged(document.scene().shapeCount() > 0);
}

void CanvasView::removeImage(qsizetype index) {
  if (index < 0 || index >= imageCount()) {
    return;
  }
  const bool removesCurrent = index == currentImageIndex_;
  if (removesCurrent) {
    setScene(nullptr);
    undoGroup_.setActiveStack(&fallbackUndoStack_);
  }
  ImageDocument& removed = *documents_[static_cast<std::size_t>(index)];
  undoGroup_.removeStack(&removed.undoStack());
  documents_.erase(documents_.begin() + index);

  if (documents_.empty()) {
    currentImageIndex_ = -1;
    fallbackUndoStack_.clear();
    setZoomFactor(1.0);
    emit cursorLeftImage();
    emit currentImageChanged(-1);
    emit imageCollectionChanged();
    emit imageAvailabilityChanged(false);
    emit currentShapeAvailabilityChanged(false);
    return;
  }
  if (removesCurrent) {
    currentImageIndex_ = -1;
    setCurrentImageIndex(std::min(index, imageCount() - 1));
  } else if (index < currentImageIndex_) {
    --currentImageIndex_;
    emit currentImageChanged(currentImageIndex_);
  }
  emit imageCollectionChanged();
}

void CanvasView::removeCurrentImage() { removeImage(currentImageIndex_); }

void CanvasView::clearImageShapes(qsizetype index) {
  if (index < 0 || index >= imageCount()) {
    return;
  }
  ImageDocument& document = *documents_[static_cast<std::size_t>(index)];
  document.scene().cancelCreation();
  if (document.scene().shapeCount() == 0) {
    return;
  }
  document.undoStack().push(new DeleteAllShapesCommand{document.scene()});
}

void CanvasView::clearCurrentImageShapes() { clearImageShapes(currentImageIndex_); }

void CanvasView::setZoomFactor(qreal factor) {
  const qreal bounded = std::clamp(factor, minimumZoom, maximumZoom);
  if (qFuzzyCompare(zoomFactor_, bounded)) {
    return;
  }
  zoomFactor_ = bounded;
  resetTransform();
  scale(zoomFactor_, zoomFactor_);
  emit zoomFactorChanged(zoomFactor_);
}

void CanvasView::setCreationMode(ShapeType type, bool enabled) {
  if (enabled) {
    creationType_ = type;
  } else if (creationType_ == type) {
    creationType_.reset();
  }
  if (ImageScene* current = imageScene(); current != nullptr) {
    current->setCreationMode(type, enabled);
  }
}

void CanvasView::rotateLeft() { rotateImage(-90.0); }

void CanvasView::rotateRight() { rotateImage(90.0); }

void CanvasView::contextMenuEvent(QContextMenuEvent* event) {
  ImageScene* current = imageScene();
  if (current == nullptr) {
    return;
  }
  if (current->consumeContextMenuSuppression()) {
    event->accept();
    return;
  }
  ShapeItem* target = current->shapeItemAt(mapToScene(event->pos()));
  QMenu menu{viewport()};
  if (target != nullptr) {
    current->clearSelection();
    target->setSelected(true);
    batchSaveRoiAction_->setEnabled(imageCount() > 1);
    menu.addActions({saveRoiAction_, batchSaveRoiAction_});
    menu.addSeparator();
    menu.addActions({cloneShapeAction_, deleteShapeAction_});
  } else {
    const bool hasShapes = current->shapeCount() > 0;
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

void CanvasView::leaveEvent(QEvent* event) {
  emit cursorLeftImage();
  QGraphicsView::leaveEvent(event);
}

void CanvasView::mouseMoveEvent(QMouseEvent* event) {
  const QPointF point = mapToScene(event->position().toPoint());
  const ImageScene* current = imageScene();
  if (current != nullptr && current->imageBounds().contains(point)) {
    emit cursorImagePositionChanged(point);
  } else {
    emit cursorLeftImage();
  }
  QGraphicsView::mouseMoveEvent(event);
}

void CanvasView::wheelEvent(QWheelEvent* event) {
  const int delta = event->angleDelta().y();
  if (!hasImage() || !event->modifiers().testFlag(Qt::ControlModifier) || delta == 0) {
    QGraphicsView::wheelEvent(event);
    return;
  }
  const qreal steps = static_cast<qreal>(delta) / wheelStepAngle;
  setZoomFactor(zoomFactor_ * std::pow(zoomPerWheelStep, steps));
  event->accept();
}

ImageDocument* CanvasView::currentDocument() noexcept {
  return currentImageIndex_ >= 0 && currentImageIndex_ < imageCount()
             ? documents_[static_cast<std::size_t>(currentImageIndex_)].get()
             : nullptr;
}

const ImageDocument* CanvasView::currentDocument() const noexcept {
  return currentImageIndex_ >= 0 && currentImageIndex_ < imageCount()
             ? documents_[static_cast<std::size_t>(currentImageIndex_)].get()
             : nullptr;
}

ImageScene* CanvasView::imageScene() noexcept {
  ImageDocument* document = currentDocument();
  return document == nullptr ? nullptr : &document->scene();
}

const ImageScene* CanvasView::imageScene() const noexcept {
  const ImageDocument* document = currentDocument();
  return document == nullptr ? nullptr : &document->scene();
}

void CanvasView::initializeContextActions() {
  saveRoiAction_ = new QAction{tr("Save ROI"), this};
  batchSaveRoiAction_ = new QAction{tr("Batch Save"), this};
  cloneShapeAction_ = new QAction{tr("Clone"), this};
  deleteShapeAction_ = new QAction{tr("Delete"), this};
  saveAllRoisAction_ = new QAction{tr("Save All"), this};
  batchSaveAllRoisAction_ = new QAction{tr("Batch Save All"), this};
  deleteAllShapesAction_ = new QAction{tr("Delete All"), this};
  saveRoiAction_->setObjectName("saveRoiAction");
  batchSaveRoiAction_->setObjectName("batchSaveRoiAction");
  cloneShapeAction_->setObjectName("cloneShapeAction");
  deleteShapeAction_->setObjectName("deleteShapeAction");
  saveAllRoisAction_->setObjectName("saveAllRoisAction");
  batchSaveAllRoisAction_->setObjectName("batchSaveAllRoisAction");
  deleteAllShapesAction_->setObjectName("deleteAllShapesAction");
  connect(saveRoiAction_, &QAction::triggered, this, &CanvasView::saveSelectedRoi);
  connect(batchSaveRoiAction_, &QAction::triggered, this, &CanvasView::batchSaveSelectedRoi);
  connect(cloneShapeAction_, &QAction::triggered, this, &CanvasView::cloneSelectedShape);
  connect(deleteShapeAction_, &QAction::triggered, this, &CanvasView::deleteSelectedShape);
  connect(saveAllRoisAction_, &QAction::triggered, this, &CanvasView::saveAllRois);
  connect(batchSaveAllRoisAction_, &QAction::triggered, this, &CanvasView::batchSaveAllRois);
  connect(deleteAllShapesAction_, &QAction::triggered, this, &CanvasView::deleteAllShapes);
}

void CanvasView::saveSelectedRoi() {
  if (ShapeItem* selected = imageScene()->selectedShapeItem(); selected != nullptr) {
    saveRois({selected});
  }
}

void CanvasView::batchSaveSelectedRoi() {
  if (ShapeItem* selected = imageScene()->selectedShapeItem(); selected != nullptr) {
    batchSaveRois({selected});
  }
}

void CanvasView::cloneSelectedShape() {
  if (ShapeItem* selected = imageScene()->selectedShapeItem(); selected != nullptr) {
    pushCommand(std::make_unique<CloneShapeCommand>(*imageScene(), *selected));
  }
}

void CanvasView::deleteSelectedShape() {
  if (ShapeItem* selected = imageScene()->selectedShapeItem(); selected != nullptr) {
    pushCommand(std::make_unique<DeleteShapeCommand>(*imageScene(), *selected));
  }
}

void CanvasView::saveAllRois() { saveRois(imageScene()->shapeItems()); }

void CanvasView::batchSaveAllRois() { batchSaveRois(imageScene()->shapeItems()); }

void CanvasView::deleteAllShapes() {
  pushCommand(std::make_unique<DeleteAllShapesCommand>(*imageScene()));
}

void CanvasView::pushCommand(std::unique_ptr<QUndoCommand> command) {
  undoStack().push(command.release());
}

void CanvasView::saveRois(const std::vector<ShapeItem*>& targets) {
  if (targets.empty() || currentDocument() == nullptr) {
    return;
  }
  const QString selected = QFileDialog::getSaveFileName(
      this, tr("Save ROI"), QDir{lastSaveDirectory_}.filePath(QStringLiteral("roi.png")),
      tr("PNG Images (*.png)"));
  if (selected.isEmpty()) {
    return;
  }
  const QString base = pngFileName(selected);
  lastSaveDirectory_ = QFileInfo{base}.absolutePath();
  QSettings{}.setValue(QStringLiteral("roi/lastSaveDirectory"), lastSaveDirectory_);
  std::vector<QString> outputs;
  outputs.reserve(targets.size());
  for (std::size_t index = 0; index < targets.size(); ++index) {
    outputs.push_back(targets.size() == 1 ? base : numberedPngFileName(base, index));
  }
  if (std::ranges::any_of(outputs, [](const QString& name) { return QFileInfo::exists(name); }) &&
      QMessageBox::question(this, tr("Overwrite ROI Files"),
                            tr("One or more ROI files already exist. Overwrite them?")) !=
          QMessageBox::Yes) {
    return;
  }
  for (std::size_t index = 0; index < targets.size(); ++index) {
    QString error;
    if (!saveRoiPng(currentDocument()->image(), targets[index]->imagePath(), outputs[index],
                    &error)) {
      QMessageBox::warning(this, tr("Unable to Save ROI"),
                           tr("Could not save %1: %2").arg(outputs[index], error));
      return;
    }
  }
}

void CanvasView::batchSaveRois(const std::vector<ShapeItem*>& targets) {
  if (targets.empty() || imageCount() < 2) {
    return;
  }
  std::vector<QPainterPath> paths;
  paths.reserve(targets.size());
  for (const ShapeItem* target : targets) {
    paths.push_back(target->imagePath());
  }
  std::vector<BatchSaveRow> rows;
  for (qsizetype index = 0; index < imageCount(); ++index) {
    const QImage& image = documents_[static_cast<std::size_t>(index)]->image();
    const bool savable = std::ranges::all_of(
        paths, [&image](const QPainterPath& path) { return isRoiWithinImage(image, path); });
    rows.push_back({.imagePath = imagePathAt(index),
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
  const QString timestamp =
      QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
  QStringList failures;
  std::size_t sequence = 0;
  for (qsizetype imageIndex = 0; imageIndex < imageCount(); ++imageIndex) {
    if (!rows[static_cast<std::size_t>(imageIndex)].savable) {
      continue;
    }
    const ImageDocument& document = *documents_[static_cast<std::size_t>(imageIndex)];
    const QString stem = QFileInfo{document.filePath()}.completeBaseName();
    for (const QPainterPath& path : paths) {
      ++sequence;
      const QString name = QDir{lastSaveDirectory_}.filePath(
          QStringLiteral("%1_roi_%2_%3.png")
              .arg(stem, timestamp)
              .arg(static_cast<qulonglong>(sequence), 3, 10, QChar{'0'}));
      QString error;
      if (!saveRoiPng(document.image(), path, name, &error)) {
        failures.push_back(tr("%1: %2").arg(name, error));
      }
    }
  }
  if (!failures.isEmpty()) {
    QMessageBox::warning(this, tr("Some ROI Files Could Not Be Saved"), failures.join('\n'));
  }
}

void CanvasView::rotateImage(qreal degrees) {
  ImageDocument* document = currentDocument();
  if (document == nullptr) {
    return;
  }
  imageScene()->cancelCreation();
  undoStack().clear();
  QTransform rotation;
  rotation.rotate(degrees);
  const QTransform shapeTransformation =
      QImage::trueMatrix(rotation, document->image().width(), document->image().height());
  imageScene()->applyImageTransform(shapeTransformation);
  document->setImage(document->image().transformed(rotation));
  emit imageThumbnailChanged(currentImageIndex_);
}

} // namespace quickshot
