#include "quickshot/image_scene.hpp"

#include "quickshot/commands/shape_commands.hpp"

#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QUndoStack>
#include <algorithm>
#include <array>
#include <ranges>
#include <utility>

namespace quickshot {
class ImageItem final : public QGraphicsItem {
public:
  explicit ImageItem(const QImage& image) : image_(image) {
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
    : QGraphicsScene(parent), image_(std::move(image)), imageItem_(new ImageItem{image_}) {
  addItem(imageItem_);
  setSceneRect(imageBounds());
}

ImageScene::~ImageScene() = default;

void ImageScene::setUndoStack(QUndoStack& undoStack) noexcept { undoStack_ = &undoStack; }

const QImage& ImageScene::image() const noexcept { return image_; }

void ImageScene::setImage(QImage image) {
  image_ = std::move(image);
  imageItem_->setImage(image_);
  setSceneRect(imageBounds());
}

QRectF ImageScene::imageBounds() const noexcept { return {QPointF{}, QSizeF{image_.size()}}; }

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
  addItem(&item);
  nextShapeZ_ = std::max(nextShapeZ_, item.zValue() + 1.0);
}

std::unique_ptr<ShapeItem> ImageScene::takeShapeItem(ShapeItem& item) {
  if (item.scene() != this) {
    return nullptr;
  }
  removeItem(&item);
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

void ImageScene::applyImageTransform(const QTransform& transformation) {
  cancelCreation();
  for (ShapeItem* item : shapeItems()) {
    item->applyImageTransform(transformation);
  }
}

void ImageScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
  if (provisionalShape_ != nullptr) {
    if (provisionalShape_->model().creationKind() == CreationKind::MultiPoint) {
      provisionalShape_->setCreationPreview(event->scenePos());
    } else {
      provisionalShape_->updateCreation(creationOrigin_, imageBounds(), event->scenePos());
    }
    event->accept();
    return;
  }
  QGraphicsScene::mouseMoveEvent(event);
}

void ImageScene::mousePressEvent(QGraphicsSceneMouseEvent* event) {
  if (provisionalShape_ != nullptr &&
      provisionalShape_->model().creationKind() == CreationKind::MultiPoint) {
    if (event->button() == Qt::RightButton) {
      finishCreation(true);
    } else if (event->button() == Qt::LeftButton && imageBounds().contains(event->scenePos())) {
      provisionalShape_->appendCreationPoint(event->scenePos());
    }
    event->accept();
    return;
  }

  if (event->button() == Qt::LeftButton && creationType_.has_value() &&
      imageBounds().contains(event->scenePos()) && shapeItemAt(event->scenePos()) == nullptr) {
    beginCreation(event->scenePos());
    event->accept();
    return;
  }
  QGraphicsScene::mousePressEvent(event);
}

void ImageScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
  if (provisionalShape_ != nullptr &&
      provisionalShape_->model().creationKind() == CreationKind::Drag &&
      event->button() == Qt::LeftButton) {
    provisionalShape_->updateCreation(creationOrigin_, imageBounds(), event->scenePos());
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
  provisionalShape_ = item.release();
  addItem(provisionalShape_);
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
  previousSelection_ = nullptr;
}

void ImageScene::pushCommand(std::unique_ptr<QUndoCommand> command) {
  Q_ASSERT(undoStack_ != nullptr);
  if (undoStack_ != nullptr) {
    undoStack_->push(command.release());
  }
}

} // namespace quickshot
