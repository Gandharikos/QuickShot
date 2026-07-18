#include "quickshot/image_scene.hpp"

#include "quickshot/shape_commands.hpp"

#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QTransform>
#include <QUndoStack>
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace quickshot {
// The image and every ROI item share this parent so one Graphics View transform rotates the whole
// document without resampling pixels or rewriting shape geometry.
class ImageContentItem final : public QGraphicsItem {
public:
  explicit ImageContentItem(const QRectF& bounds) : bounds_(bounds) {
    setAcceptedMouseButtons(Qt::NoButton);
    setFlag(QGraphicsItem::ItemHasNoContents);
    setTransformOriginPoint(bounds_.center());
  }

  [[nodiscard]] QRectF boundingRect() const override { return bounds_; }

  void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override {}

  void setBounds(const QRectF& bounds) {
    prepareGeometryChange();
    bounds_ = bounds;
    setTransformOriginPoint(bounds_.center());
  }

private:
  QRectF bounds_;
};

class ImageItem final : public QGraphicsItem {
public:
  ImageItem(QImage image, QGraphicsItem* parent) : QGraphicsItem(parent), image_(std::move(image)) {
    setAcceptedMouseButtons(Qt::NoButton);
    setZValue(-1.0);
  }

  [[nodiscard]] QRectF boundingRect() const override { return {QPointF{}, QSizeF{image_.size()}}; }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->drawImage(QPointF{}, image_);
  }

  void setImage(const QImage& image) {
    prepareGeometryChange();
    image_ = image;
    update();
  }

private:
  QImage image_;
};

namespace {

constexpr qreal minimumShapeSize = 1.0;
constexpr qreal cloneOffset = 10.0;

bool hasUsableArea(const ShapeItem& item) {
  const QRectF bounds = item.imagePath().boundingRect();
  return bounds.width() >= minimumShapeSize && bounds.height() >= minimumShapeSize;
}

} // namespace

ImageScene::ImageScene(QImage image, QObject* parent)
    : QGraphicsScene(parent), image_(std::move(image)),
      contentItem_(new ImageContentItem{imageBounds()}),
      imageItem_(new ImageItem{image_, contentItem_}) {
  addItem(contentItem_);
  updateSceneRect();
}

ImageScene::~ImageScene() = default;

void ImageScene::setUndoStack(QUndoStack& undoStack) noexcept { undoStack_ = &undoStack; }

const QImage& ImageScene::image() const noexcept { return image_; }

void ImageScene::setImage(QImage image) {
  image_ = std::move(image);
  imageItem_->setImage(image_);
  contentItem_->setBounds(imageBounds());
  updateSceneRect();
}

QRectF ImageScene::imageBounds() const noexcept { return {QPointF{}, QSizeF{image_.size()}}; }

QPointF ImageScene::imagePosition(const QPointF& scenePosition) const {
  return contentItem_->mapFromScene(scenePosition);
}

QPointF ImageScene::imageCenterInScene() const {
  return contentItem_->mapToScene(imageBounds().center());
}

QTransform ImageScene::displayTransform() const {
  QTransform rotation;
  rotation.rotate(rotationDegrees());
  return QImage::trueMatrix(rotation, image_.width(), image_.height());
}

QImage ImageScene::displayImage() const {
  if (qFuzzyIsNull(rotationDegrees())) {
    return image_;
  }
  QTransform rotation;
  rotation.rotate(rotationDegrees());
  return image_.transformed(rotation, Qt::SmoothTransformation);
}

qreal ImageScene::rotationDegrees() const noexcept { return contentItem_->rotation(); }

void ImageScene::setRotationDegrees(qreal degrees) {
  qreal normalized = std::remainder(degrees, 360.0);
  if (qFuzzyIsNull(normalized)) {
    normalized = 0.0;
  }
  if (qFuzzyIsNull(rotationDegrees() - normalized)) {
    return;
  }
  contentItem_->setRotation(normalized);
  updateSceneRect();
}

void ImageScene::setCreationMode(ShapeType type, bool enabled) {
  if (enabled) {
    if (creationType_ != type) {
      cancelCreation();
    }
    creationType_ = type;
  } else if (creationType_ == type) {
    cancelCreation();
    creationType_.reset();
  }
}

void ImageScene::cancelCreation() {
  if (provisionalShape_ != nullptr) {
    removeItem(provisionalShape_);
    delete provisionalShape_;
    provisionalShape_ = nullptr;
    if (previousSelection_ != nullptr && previousSelection_->scene() == this) {
      previousSelection_->setSelected(true);
    }
  }
  previousSelection_ = nullptr;
}

qsizetype ImageScene::shapeCount() const noexcept {
  return static_cast<qsizetype>(shapeItems().size());
}

ShapeItem* ImageScene::shapeItemAt(qsizetype index) const {
  const std::vector<ShapeItem*> shapes = shapeItems();
  return index >= 0 && std::cmp_less(index, shapes.size()) ? shapes[static_cast<std::size_t>(index)]
                                                           : nullptr;
}

ShapeItem* ImageScene::shapeItemAt(const QPointF& scenePosition) const {
  for (QGraphicsItem* item : items(scenePosition, Qt::IntersectsItemShape, Qt::DescendingOrder)) {
    for (QGraphicsItem* candidate = item; candidate != nullptr;
         candidate = candidate->parentItem()) {
      if (auto* shapeItem = dynamic_cast<ShapeItem*>(candidate); shapeItem != nullptr) {
        return shapeItem;
      }
    }
  }
  return nullptr;
}

ShapeItem* ImageScene::selectedShapeItem() const {
  for (QGraphicsItem* item : selectedItems()) {
    if (auto* shapeItem = dynamic_cast<ShapeItem*>(item); shapeItem != nullptr) {
      return shapeItem;
    }
  }
  return nullptr;
}

std::vector<ShapeItem*> ImageScene::shapeItems() const {
  std::vector<ShapeItem*> shapes;
  for (QGraphicsItem* item : items(Qt::AscendingOrder)) {
    if (auto* shapeItem = dynamic_cast<ShapeItem*>(item); shapeItem != nullptr) {
      shapes.push_back(shapeItem);
    }
  }
  return shapes;
}

std::unique_ptr<ShapeItem> ImageScene::makeOffsetClone(const ShapeItem& source) const {
  constexpr std::array offsets = {QPointF{-cloneOffset, 0.0}, QPointF{cloneOffset, 0.0},
                                  QPointF{0.0, -cloneOffset}, QPointF{0.0, cloneOffset}};
  const QRectF sourceBounds = source.imagePath().boundingRect();
  for (const QPointF& offset : offsets) {
    const QRectF candidate = sourceBounds.translated(offset);
    if (!imageBounds().contains(candidate)) {
      continue;
    }
    std::unique_ptr<ShapeItem> clone = source.clone();
    clone->model().moveBy(offset);
    clone->setZValue(nextShapeZ_);
    return clone;
  }
  return nullptr;
}

void ImageScene::addShapeItem(ShapeItem& item) {
  item.setParentItem(contentItem_);
  nextShapeZ_ = std::max(nextShapeZ_, item.zValue() + 1.0);
  emit shapeCollectionChanged();
}

std::unique_ptr<ShapeItem> ImageScene::takeShapeItem(ShapeItem& item) {
  if (item.scene() != this) {
    return nullptr;
  }
  item.setParentItem(nullptr);
  removeItem(&item);
  emit shapeCollectionChanged();
  return std::unique_ptr<ShapeItem>{&item};
}

void ImageScene::commitTransform(ShapeItem& item, ShapeItemGeometry before, ShapeItemGeometry after,
                                 const QString& text) {
  if (before.shape == nullptr || after.shape == nullptr ||
      (before.position == after.position && before.shape->equals(*after.shape))) {
    return;
  }
  pushCommand(std::make_unique<TransformShapeCommand>(*this, item, std::move(before),
                                                      std::move(after), text));
}

void ImageScene::suppressNextContextMenu() noexcept { suppressContextMenu_ = true; }

bool ImageScene::consumeContextMenuSuppression() noexcept {
  return std::exchange(suppressContextMenu_, false);
}

void ImageScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
  if (provisionalShape_ != nullptr) {
    const QPointF point = imagePosition(event->scenePos());
    if (provisionalShape_->model().creationKind() == CreationKind::MultiPoint) {
      provisionalShape_->setCreationPreview(point);
    } else {
      provisionalShape_->updateCreation(creationOrigin_, imageBounds(), point);
    }
    event->accept();
    return;
  }
  QGraphicsScene::mouseMoveEvent(event);
}

void ImageScene::mousePressEvent(QGraphicsSceneMouseEvent* event) {
  const QPointF point = imagePosition(event->scenePos());
  if (provisionalShape_ != nullptr &&
      provisionalShape_->model().creationKind() == CreationKind::MultiPoint) {
    if (event->button() == Qt::RightButton) {
      finishCreation(true);
    } else if (event->button() == Qt::LeftButton && imageBounds().contains(point)) {
      provisionalShape_->appendCreationPoint(point);
    }
    event->accept();
    return;
  }

  if (event->button() == Qt::LeftButton && creationType_.has_value() &&
      imageBounds().contains(point) && shapeItemAt(event->scenePos()) == nullptr) {
    beginCreation(point);
    event->accept();
    return;
  }
  QGraphicsScene::mousePressEvent(event);
}

void ImageScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
  if (provisionalShape_ != nullptr &&
      provisionalShape_->model().creationKind() == CreationKind::Drag &&
      event->button() == Qt::LeftButton) {
    provisionalShape_->updateCreation(creationOrigin_, imageBounds(),
                                      imagePosition(event->scenePos()));
    finishCreation(false);
    event->accept();
    return;
  }
  QGraphicsScene::mouseReleaseEvent(event);
}

void ImageScene::beginCreation(const QPointF& position) {
  if (!creationType_.has_value()) {
    return;
  }
  const ShapeType creationType = creationType_.value();
  previousSelection_ = selectedShapeItem();
  clearSelection();
  auto item = std::make_unique<ShapeItem>(Shape::make(creationType, QRectF{position, position}));
  item->setZValue(nextShapeZ_++);
  item->setParentItem(contentItem_);
  provisionalShape_ = item.release();
  provisionalShape_->setSelected(true);
  creationOrigin_ = position;
}

void ImageScene::finishCreation(bool suppressContextMenu) {
  Q_ASSERT(provisionalShape_ != nullptr);
  if (provisionalShape_ == nullptr) {
    return;
  }
  if (provisionalShape_->model().creationKind() == CreationKind::MultiPoint) {
    provisionalShape_->finishCreation();
  }
  ShapeItem* completed = provisionalShape_;
  provisionalShape_ = nullptr;
  if (suppressContextMenu) {
    suppressNextContextMenu();
  }
  if (!completed->model().isCreationComplete() || !hasUsableArea(*completed)) {
    removeItem(completed);
    delete completed;
    if (previousSelection_ != nullptr && previousSelection_->scene() == this) {
      previousSelection_->setSelected(true);
    }
    previousSelection_ = nullptr;
    return;
  }
  pushCommand(std::make_unique<CreateShapeCommand>(*this, *completed, previousSelection_));
  emit shapeCollectionChanged();
  previousSelection_ = nullptr;
}

void ImageScene::pushCommand(std::unique_ptr<QUndoCommand> command) {
  Q_ASSERT(undoStack_ != nullptr);
  if (undoStack_ != nullptr) {
    undoStack_->push(command.release());
  }
}

void ImageScene::updateSceneRect() {
  setSceneRect(contentItem_->mapRectToScene(imageBounds()).normalized());
}

} // namespace quickshot
