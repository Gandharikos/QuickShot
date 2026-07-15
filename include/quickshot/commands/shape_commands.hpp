#pragma once

#include "quickshot/shapes/shape.hpp"

#include <QUndoCommand>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace quickshot {

class QDrawWidget;

class ShapeCommand : public QUndoCommand {
public:
  ~ShapeCommand() override = default;

  ShapeCommand(const ShapeCommand&) = delete;
  ShapeCommand& operator=(const ShapeCommand&) = delete;
  ShapeCommand(ShapeCommand&&) = delete;
  ShapeCommand& operator=(ShapeCommand&&) = delete;

protected:
  ShapeCommand(QDrawWidget& drawWidget, const QString& text);

  [[nodiscard]] std::unique_ptr<Shape> makeOffsetClone(const Shape& shape) const;
  [[nodiscard]] std::optional<std::size_t> indexOf(const Shape& shape) const;
  [[nodiscard]] std::size_t shapeCount() const noexcept;
  [[nodiscard]] std::unique_ptr<Shape> takeShape(std::size_t index);
  void insertShape(std::size_t index, std::unique_ptr<Shape> shape);
  [[nodiscard]] std::vector<std::unique_ptr<Shape>> takeAllShapes();
  void restoreShapes(std::vector<std::unique_ptr<Shape>> shapes);
  [[nodiscard]] Shape* selectedShape() const noexcept;
  void setSelectedShape(Shape* shape) noexcept;
  void applyGeometry(Shape& shape, const ShapeGeometry& geometry);
  void updateViewport();

private:
  QDrawWidget& drawWidget_;
};

class CloneShapeCommand final : public ShapeCommand {
public:
  CloneShapeCommand(QDrawWidget& drawWidget, const Shape& source);

  void undo() override;
  void redo() override;

private:
  std::unique_ptr<Shape> clone_;
  Shape* previousSelection_ = nullptr;
  std::size_t insertionIndex_ = 0;
};

class CreateShapeCommand final : public ShapeCommand {
public:
  CreateShapeCommand(QDrawWidget& drawWidget, std::unique_ptr<Shape> shape,
                     std::size_t insertionIndex, Shape* previousSelection);

  void undo() override;
  void redo() override;

private:
  std::unique_ptr<Shape> shape_;
  Shape* previousSelection_ = nullptr;
  std::size_t insertionIndex_ = 0;
};

class TransformShapeCommand final : public ShapeCommand {
public:
  TransformShapeCommand(QDrawWidget& drawWidget, Shape& shape,
                        std::unique_ptr<ShapeGeometry> before, std::unique_ptr<ShapeGeometry> after,
                        const QString& text);

  void undo() override;
  void redo() override;

private:
  Shape& shape_;
  std::unique_ptr<ShapeGeometry> before_;
  std::unique_ptr<ShapeGeometry> after_;
};

class DeleteShapeCommand final : public ShapeCommand {
public:
  DeleteShapeCommand(QDrawWidget& drawWidget, const Shape& shape);

  void undo() override;
  void redo() override;

private:
  std::unique_ptr<Shape> deletedShape_;
  Shape* previousSelection_ = nullptr;
  Shape* selectionAfterDeletion_ = nullptr;
  std::optional<std::size_t> deletionIndex_;
};

class DeleteAllShapesCommand final : public ShapeCommand {
public:
  explicit DeleteAllShapesCommand(QDrawWidget& drawWidget);

  void undo() override;
  void redo() override;

private:
  std::vector<std::unique_ptr<Shape>> deletedShapes_;
  Shape* previousSelection_ = nullptr;
};

} // namespace quickshot
