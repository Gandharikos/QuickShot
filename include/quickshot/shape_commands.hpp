#pragma once

#include "quickshot/shape_item.hpp"

#include <QUndoCommand>
#include <memory>
#include <vector>

namespace quickshot {

class ImageScene;

class ShapeCommand : public QUndoCommand {
public:
  ~ShapeCommand() override = default;

  ShapeCommand(const ShapeCommand&) = delete;
  ShapeCommand& operator=(const ShapeCommand&) = delete;
  ShapeCommand(ShapeCommand&&) = delete;
  ShapeCommand& operator=(ShapeCommand&&) = delete;

protected:
  ShapeCommand(ImageScene& scene, const QString& text);
  void select(ShapeItem* item);
  [[nodiscard]] std::unique_ptr<ShapeItem> take(ShapeItem& item);
  void restore(std::unique_ptr<ShapeItem>& item);

  ImageScene& scene_;
};

class CloneShapeCommand final : public ShapeCommand {
public:
  CloneShapeCommand(ImageScene& scene, const ShapeItem& source);
  void undo() override;
  void redo() override;

private:
  std::unique_ptr<ShapeItem> detached_;
  ShapeItem* item_ = nullptr;
  ShapeItem* previousSelection_ = nullptr;
};

class CreateShapeCommand final : public ShapeCommand {
public:
  CreateShapeCommand(ImageScene& scene, ShapeItem& shape, ShapeItem* previousSelection);
  void undo() override;
  void redo() override;

private:
  std::unique_ptr<ShapeItem> detached_;
  ShapeItem* item_ = nullptr;
  ShapeItem* previousSelection_ = nullptr;
  bool firstRedo_ = true;
};

class TransformShapeCommand final : public ShapeCommand {
public:
  TransformShapeCommand(ImageScene& scene, ShapeItem& shape, ShapeItemGeometry before,
                        ShapeItemGeometry after, const QString& text);
  void undo() override;
  void redo() override;

private:
  ShapeItem& item_;
  ShapeItemGeometry before_;
  ShapeItemGeometry after_;
};

class DeleteShapeCommand final : public ShapeCommand {
public:
  DeleteShapeCommand(ImageScene& scene, ShapeItem& shape);
  void undo() override;
  void redo() override;

private:
  std::unique_ptr<ShapeItem> detached_;
  ShapeItem* item_ = nullptr;
  ShapeItem* previousSelection_ = nullptr;
};

class DeleteAllShapesCommand final : public ShapeCommand {
public:
  explicit DeleteAllShapesCommand(ImageScene& scene);
  void undo() override;
  void redo() override;

private:
  std::vector<std::unique_ptr<ShapeItem>> detached_;
  std::vector<ShapeItem*> items_;
  ShapeItem* previousSelection_ = nullptr;
};

} // namespace quickshot
