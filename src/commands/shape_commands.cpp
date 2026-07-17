#include "quickshot/commands/shape_commands.hpp"

#include "quickshot/image_scene.hpp"

#include <QCoreApplication>
#include <utility>

namespace quickshot {

ShapeCommand::ShapeCommand(ImageScene& scene, const QString& text)
    : QUndoCommand(text), scene_(scene) {}

void ShapeCommand::select(ShapeItem* item) {
  scene_.clearSelection();
  if (item != nullptr && item->scene() == &scene_) {
    item->setSelected(true);
  }
}

std::unique_ptr<ShapeItem> ShapeCommand::take(ShapeItem& item) {
  scene_.cancelCreation();
  return scene_.takeShapeItem(item);
}

void ShapeCommand::restore(std::unique_ptr<ShapeItem>& item) {
  scene_.cancelCreation();
  if (item == nullptr) {
    return;
  }
  ShapeItem* restored = item.release();
  scene_.addShapeItem(*restored);
}

CloneShapeCommand::CloneShapeCommand(ImageScene& scene, const ShapeItem& source)
    : ShapeCommand(scene, QCoreApplication::translate("CloneShapeCommand", "Clone Shape")),
      detached_(scene.makeOffsetClone(source)), item_(detached_.get()),
      previousSelection_(scene.selectedShapeItem()) {
  setObsolete(detached_ == nullptr);
}

void CloneShapeCommand::undo() {
  if (item_ != nullptr) {
    detached_ = take(*item_);
    select(previousSelection_);
  }
}

void CloneShapeCommand::redo() {
  restore(detached_);
  select(item_);
}

CreateShapeCommand::CreateShapeCommand(ImageScene& scene, ShapeItem& shape,
                                       ShapeItem* previousSelection)
    : ShapeCommand(scene, QCoreApplication::translate("CreateShapeCommand", "Create Shape")),
      item_(&shape), previousSelection_(previousSelection) {}

void CreateShapeCommand::undo() {
  detached_ = take(*item_);
  select(previousSelection_);
}

void CreateShapeCommand::redo() {
  if (firstRedo_) {
    firstRedo_ = false;
  } else {
    restore(detached_);
  }
  select(item_);
}

TransformShapeCommand::TransformShapeCommand(ImageScene& scene, ShapeItem& shape,
                                             ShapeItemGeometry before, ShapeItemGeometry after,
                                             const QString& text)
    : ShapeCommand(scene, text), item_(shape), before_(std::move(before)),
      after_(std::move(after)) {}

void TransformShapeCommand::undo() { item_.restoreGeometry(before_); }

void TransformShapeCommand::redo() { item_.restoreGeometry(after_); }

DeleteShapeCommand::DeleteShapeCommand(ImageScene& scene, ShapeItem& shape)
    : ShapeCommand(scene, QCoreApplication::translate("DeleteShapeCommand", "Delete Shape")),
      item_(&shape), previousSelection_(scene.selectedShapeItem()) {}

void DeleteShapeCommand::undo() {
  restore(detached_);
  select(previousSelection_);
}

void DeleteShapeCommand::redo() {
  detached_ = take(*item_);
  select(nullptr);
}

DeleteAllShapesCommand::DeleteAllShapesCommand(ImageScene& scene)
    : ShapeCommand(scene,
                   QCoreApplication::translate("DeleteAllShapesCommand", "Delete All Shapes")),
      items_(scene.shapeItems()), previousSelection_(scene.selectedShapeItem()) {
  setObsolete(items_.empty());
}

void DeleteAllShapesCommand::undo() {
  for (std::unique_ptr<ShapeItem>& item : detached_) {
    restore(item);
  }
  detached_.clear();
  select(previousSelection_);
}

void DeleteAllShapesCommand::redo() {
  detached_.clear();
  detached_.reserve(items_.size());
  for (ShapeItem* item : items_) {
    if (item != nullptr && item->scene() == &scene_) {
      detached_.push_back(take(*item));
    }
  }
  select(nullptr);
}

} // namespace quickshot
