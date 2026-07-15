#include "quickshot/commands/shape_commands.hpp"

#include "quickshot/drag_state.hpp"
#include "quickshot/qdrawwidget.hpp"

#include <QCoreApplication>
#include <algorithm>
#include <iterator>
#include <ranges>
#include <utility>

namespace quickshot {

ShapeCommand::ShapeCommand(QDrawWidget& drawWidget, const QString& text)
    : QUndoCommand(text), drawWidget_(drawWidget) {}

std::unique_ptr<Shape> ShapeCommand::makeOffsetClone(const Shape& shape) const {
  return drawWidget_.makeOffsetClone(shape);
}

std::optional<std::size_t> ShapeCommand::indexOf(const Shape& shape) const {
  const auto position =
      std::ranges::find_if(drawWidget_.shapes_, [&shape](const std::unique_ptr<Shape>& candidate) {
        return candidate.get() == &shape;
      });
  if (position == drawWidget_.shapes_.end()) {
    return std::nullopt;
  }

  return static_cast<std::size_t>(std::distance(drawWidget_.shapes_.begin(), position));
}

std::size_t ShapeCommand::shapeCount() const noexcept { return drawWidget_.shapes_.size(); }

std::unique_ptr<Shape> ShapeCommand::takeShape(std::size_t index) {
  if (index >= drawWidget_.shapes_.size()) {
    return nullptr;
  }

  const auto position = std::next(drawWidget_.shapes_.begin(), static_cast<std::ptrdiff_t>(index));
  drawWidget_.dragState_.reset();
  std::unique_ptr<Shape> shape = std::move(*position);
  if (drawWidget_.selectedShape_ == shape.get()) {
    drawWidget_.selectedShape_ = nullptr;
  }
  drawWidget_.shapes_.erase(position);
  return shape;
}

void ShapeCommand::insertShape(std::size_t index, std::unique_ptr<Shape> shape) {
  const auto position = std::next(drawWidget_.shapes_.begin(), static_cast<std::ptrdiff_t>(index));
  drawWidget_.shapes_.insert(position, std::move(shape));
}

std::vector<std::unique_ptr<Shape>> ShapeCommand::takeAllShapes() {
  drawWidget_.dragState_.reset();
  drawWidget_.selectedShape_ = nullptr;
  return std::exchange(drawWidget_.shapes_, {});
}

void ShapeCommand::restoreShapes(std::vector<std::unique_ptr<Shape>> shapes) {
  drawWidget_.shapes_ = std::move(shapes);
}

Shape* ShapeCommand::selectedShape() const noexcept { return drawWidget_.selectedShape_; }

void ShapeCommand::setSelectedShape(Shape* shape) noexcept { drawWidget_.selectedShape_ = shape; }

void ShapeCommand::updateViewport() { drawWidget_.viewport()->update(); }

CloneShapeCommand::CloneShapeCommand(QDrawWidget& drawWidget, const Shape& source)
    : ShapeCommand(drawWidget, QCoreApplication::translate("CloneShapeCommand", "Clone Shape")),
      clone_(makeOffsetClone(source)), previousSelection_(selectedShape()),
      insertionIndex_(shapeCount()) {
  setObsolete(clone_ == nullptr);
}

void CloneShapeCommand::undo() {
  clone_ = takeShape(insertionIndex_);
  setSelectedShape(previousSelection_);
  updateViewport();
}

void CloneShapeCommand::redo() {
  if (clone_ == nullptr) {
    return;
  }

  Shape* clone = clone_.get();
  insertShape(insertionIndex_, std::move(clone_));
  setSelectedShape(clone);
  updateViewport();
}

DeleteShapeCommand::DeleteShapeCommand(QDrawWidget& drawWidget, const Shape& shape)
    : ShapeCommand(drawWidget, QCoreApplication::translate("DeleteShapeCommand", "Delete Shape")),
      previousSelection_(selectedShape()),
      selectionAfterDeletion_(previousSelection_ == &shape ? nullptr : previousSelection_) {
  const std::optional<std::size_t> index = indexOf(shape);
  if (!index.has_value()) {
    setObsolete(true);
    return;
  }

  deletionIndex_ = index;
}

void DeleteShapeCommand::undo() {
  if (!deletionIndex_.has_value() || deletedShape_ == nullptr) {
    return;
  }

  insertShape(*deletionIndex_, std::move(deletedShape_));
  setSelectedShape(previousSelection_);
  updateViewport();
}

void DeleteShapeCommand::redo() {
  if (!deletionIndex_.has_value()) {
    return;
  }

  deletedShape_ = takeShape(*deletionIndex_);
  setSelectedShape(selectionAfterDeletion_);
  updateViewport();
}

DeleteAllShapesCommand::DeleteAllShapesCommand(QDrawWidget& drawWidget)
    : ShapeCommand(drawWidget,
                   QCoreApplication::translate("DeleteAllShapesCommand", "Delete All Shapes")),
      previousSelection_(selectedShape()) {
  setObsolete(shapeCount() == 0);
}

void DeleteAllShapesCommand::undo() {
  restoreShapes(std::move(deletedShapes_));
  setSelectedShape(previousSelection_);
  updateViewport();
}

void DeleteAllShapesCommand::redo() {
  deletedShapes_ = takeAllShapes();
  updateViewport();
}

} // namespace quickshot
