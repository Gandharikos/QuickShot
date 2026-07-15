#include "quickshot/drag_controller.hpp"
#include "quickshot/shapes/rectangle.hpp"

#include <QPointF>
#include <QRectF>
#include <QTest>
#include <utility>

namespace {

quickshot::DragCompletion requireCompletion(std::optional<quickshot::DragCompletion> completion) {
  if (completion.has_value()) {
    return std::move(completion).value();
  }

  QTest::qFail("Expected an active drag to produce a completion", __FILE__, __LINE__);
  return {
      .result = quickshot::DragResult::RemoveShape,
      .createsShape = true,
      .shape = nullptr,
      .previousSelection = nullptr,
      .before = {},
      .after = {},
      .undoText = {},
  };
}

quickshot::ShapeHandle requireHandle(std::optional<quickshot::ShapeHandle> handle) {
  if (handle.has_value()) {
    return *handle;
  }

  QTest::qFail("Expected an active handle", __FILE__, __LINE__);
  return quickshot::ShapeHandle{0};
}

} // namespace

class DragControllerTest final : public QObject {
  Q_OBJECT

private slots:
  void statesAreSingletons();
  void createStateClampsAndValidatesBounds();
  void cancelRestoresTheInitialGeometry();
  void moveStateConstrainsShapeToImage();
  void resizeStateUpdatesTheActiveHandle();
  void rotateStateTracksTheMouseAngle();
};

void DragControllerTest::statesAreSingletons() {
  QCOMPARE(&quickshot::CreateState::instance(), &quickshot::CreateState::instance());
  QCOMPARE(&quickshot::MoveState::instance(), &quickshot::MoveState::instance());
  QCOMPARE(&quickshot::ResizeState::instance(), &quickshot::ResizeState::instance());
  QCOMPARE(&quickshot::RotateState::instance(), &quickshot::RotateState::instance());
}

void DragControllerTest::createStateClampsAndValidatesBounds() {
  quickshot::Rectangle rectangle{QRectF{20.0, 20.0, 0.0, 0.0}};
  quickshot::DragController controller;
  controller.begin(quickshot::CreateState::instance(), {.shape = rectangle,
                                                        .origin = {20.0, 20.0},
                                                        .imageBounds = {0.0, 0.0, 100.0, 80.0},
                                                        .handle = std::nullopt});

  QVERIFY(controller.isActive());
  QCOMPARE(controller.completionButton(), Qt::LeftButton);
  QVERIFY(!controller.activeHandle().has_value());
  const quickshot::DragCompletion emptyCompletion = requireCompletion(controller.finish());
  QCOMPARE(emptyCompletion.result, quickshot::DragResult::RemoveShape);
  QVERIFY(!controller.isActive());

  controller.begin(quickshot::CreateState::instance(), {.shape = rectangle,
                                                        .origin = {20.0, 20.0},
                                                        .imageBounds = {0.0, 0.0, 100.0, 80.0},
                                                        .handle = std::nullopt});
  controller.update({120.0, -10.0});

  QCOMPARE(rectangle.boundingRect(), QRectF(20.0, 0.0, 80.0, 20.0));
  const quickshot::DragCompletion completion = requireCompletion(controller.finish());
  QVERIFY(completion.createsShape);
  QCOMPARE(completion.result, quickshot::DragResult::KeepShape);
}

void DragControllerTest::cancelRestoresTheInitialGeometry() {
  quickshot::Rectangle rectangle{QRectF{10.0, 20.0, 30.0, 40.0}};
  quickshot::DragController controller;
  controller.begin(quickshot::MoveState::instance(), {.shape = rectangle,
                                                      .origin = {20.0, 30.0},
                                                      .imageBounds = {0.0, 0.0, 100.0, 80.0},
                                                      .handle = std::nullopt});
  controller.update({50.0, 50.0});

  const quickshot::DragCompletion cancellation = requireCompletion(controller.cancel());

  QVERIFY(!controller.isActive());
  QVERIFY(!cancellation.createsShape);
  QCOMPARE(rectangle.boundingRect(), QRectF(10.0, 20.0, 30.0, 40.0));
}

void DragControllerTest::moveStateConstrainsShapeToImage() {
  quickshot::Rectangle rectangle{QRectF{10.0, 20.0, 30.0, 40.0}};
  quickshot::DragController controller;
  controller.begin(quickshot::MoveState::instance(), {.shape = rectangle,
                                                      .origin = {20.0, 30.0},
                                                      .imageBounds = {0.0, 0.0, 100.0, 80.0},
                                                      .handle = std::nullopt});

  controller.update({200.0, 200.0});
  QCOMPARE(rectangle.boundingRect(), QRectF(70.0, 40.0, 30.0, 40.0));

  controller.update({-100.0, -100.0});
  QCOMPARE(rectangle.boundingRect(), QRectF(0.0, 0.0, 30.0, 40.0));

  const quickshot::DragCompletion completion = requireCompletion(controller.finish());
  QVERIFY(completion.before != nullptr);
  QVERIFY(completion.after != nullptr);
  rectangle.restoreGeometry(*completion.before);
  QCOMPARE(rectangle.boundingRect(), QRectF(10.0, 20.0, 30.0, 40.0));
  rectangle.restoreGeometry(*completion.after);
  QCOMPARE(rectangle.boundingRect(), QRectF(0.0, 0.0, 30.0, 40.0));
}

void DragControllerTest::resizeStateUpdatesTheActiveHandle() {
  quickshot::Rectangle rectangle{QRectF{10.0, 20.0, 30.0, 40.0}};
  quickshot::DragController controller;
  controller.begin(quickshot::ResizeState::instance(),
                   {.shape = rectangle,
                    .origin = {40.0, 60.0},
                    .imageBounds = {0.0, 0.0, 100.0, 80.0},
                    .handle = quickshot::ShapeHandle{quickshot::HandlePosition::BottomRight}});

  QCOMPARE(requireHandle(controller.activeHandle()).id(),
           quickshot::ShapeHandle{quickshot::HandlePosition::BottomRight}.id());
  QCOMPARE(controller.completionButton(), Qt::LeftButton);
  controller.update({80.0, 70.0});

  QCOMPARE(rectangle.boundingRect(), QRectF(10.0, 20.0, 70.0, 50.0));
}

void DragControllerTest::rotateStateTracksTheMouseAngle() {
  quickshot::Rectangle rectangle{QRectF{10.0, 20.0, 60.0, 40.0}};
  quickshot::DragController controller;
  controller.begin(quickshot::RotateState::instance(),
                   {.shape = rectangle,
                    .origin = {70.0, 40.0},
                    .imageBounds = {0.0, 0.0, 100.0, 80.0},
                    .handle = quickshot::ShapeHandle{quickshot::HandlePosition::Right}});

  QCOMPARE(requireHandle(controller.activeHandle()).id(),
           quickshot::ShapeHandle{quickshot::HandlePosition::Right}.id());
  QCOMPARE(controller.completionButton(), Qt::RightButton);
  controller.update({40.0, 70.0});

  QVERIFY(qAbs(rectangle.rotationDegrees() - 90.0) < 0.0001);
}

QTEST_MAIN(DragControllerTest)

#include "drag_controller_test.moc"
