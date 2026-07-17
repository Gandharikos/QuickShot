#include "quickshot/image_scene.hpp"
#include "quickshot/shape_commands.hpp"
#include "quickshot/shape_item.hpp"
#include "quickshot/shapes/shape.hpp"

#include <QGraphicsItem>
#include <QTest>
#include <QUndoStack>
#include <memory>

namespace {

quickshot::ShapeItem& addRectangle(quickshot::ImageScene& scene, const QRectF& bounds) {
  auto item = std::make_unique<quickshot::ShapeItem>(
      quickshot::Shape::make(quickshot::ShapeType::Rectangle, bounds));
  quickshot::ShapeItem* result = item.release();
  scene.addShapeItem(*result);
  return *result;
}

class GraphicsSceneTest final : public QObject {
  Q_OBJECT

private slots:
  void shapeItemsOwnScreenSizedHandles();
  void deleteCommandTransfersOwnershipAcrossUndoRedo();
  void documentsKeepIndependentSceneState();
};

void GraphicsSceneTest::shapeItemsOwnScreenSizedHandles() {
  quickshot::ImageScene scene{QImage{200, 150, QImage::Format_RGB32}};
  QUndoStack undoStack;
  scene.setUndoStack(undoStack);
  quickshot::ShapeItem& item = addRectangle(scene, {20.0, 30.0, 80.0, 60.0});

  QCOMPARE(item.childItems().size(), 8);
  for (const QGraphicsItem* handle : item.childItems()) {
    QVERIFY(handle->flags().testFlag(QGraphicsItem::ItemIgnoresTransformations));
  }
}

void GraphicsSceneTest::deleteCommandTransfersOwnershipAcrossUndoRedo() {
  quickshot::ImageScene scene{QImage{200, 150, QImage::Format_RGB32}};
  QUndoStack undoStack;
  scene.setUndoStack(undoStack);
  quickshot::ShapeItem& item = addRectangle(scene, {20.0, 30.0, 80.0, 60.0});
  item.setSelected(true);

  undoStack.push(new quickshot::DeleteShapeCommand{scene, item});
  QCOMPARE(scene.shapeCount(), qsizetype{0});
  undoStack.undo();
  QCOMPARE(scene.shapeCount(), qsizetype{1});
  QCOMPARE(scene.selectedShapeItem(), &item);
  undoStack.redo();
  QCOMPARE(scene.shapeCount(), qsizetype{0});
}

void GraphicsSceneTest::documentsKeepIndependentSceneState() {
  quickshot::ImageScene first{QImage{200, 150, QImage::Format_RGB32}};
  quickshot::ImageScene second{QImage{100, 80, QImage::Format_RGB32}};
  QUndoStack firstUndoStack;
  QUndoStack secondUndoStack;
  first.setUndoStack(firstUndoStack);
  second.setUndoStack(secondUndoStack);
  addRectangle(first, {20.0, 30.0, 80.0, 60.0});

  QCOMPARE(first.shapeCount(), qsizetype{1});
  QCOMPARE(second.shapeCount(), qsizetype{0});
  QCOMPARE(first.imageBounds(), QRectF(0.0, 0.0, 200.0, 150.0));
  QCOMPARE(second.imageBounds(), QRectF(0.0, 0.0, 100.0, 80.0));
}

} // namespace

QTEST_MAIN(GraphicsSceneTest)

#include "graphics_scene_test.moc"
