#include "quickshot/drag_state.hpp"
#include "quickshot/rectangle.hpp"

#include <QPointF>
#include <QRectF>
#include <QTest>

class DragStateTest final : public QObject {
  Q_OBJECT

private slots:
  void createStateClampsAndValidatesBounds();
  void moveStateConstrainsShapeToImage();
  void resizeStateUpdatesTheActiveHandle();
  void rotateStateTracksTheMouseAngle();
};

void DragStateTest::createStateClampsAndValidatesBounds() {
  quickshot::Rectangle rectangle{QRectF{20.0, 20.0, 0.0, 0.0}};
  quickshot::CreateState state{rectangle, {20.0, 20.0}, {0.0, 0.0, 100.0, 80.0}};

  QCOMPARE(state.completionButton(), Qt::LeftButton);
  QVERIFY(!state.activeHandle().has_value());
  QCOMPARE(state.finish(), quickshot::DragResult::RemoveShape);

  state.update({120.0, -10.0});

  QCOMPARE(rectangle.boundingRect(), QRectF(20.0, 0.0, 80.0, 20.0));
  QCOMPARE(state.finish(), quickshot::DragResult::KeepShape);
}

void DragStateTest::moveStateConstrainsShapeToImage() {
  quickshot::Rectangle rectangle{QRectF{10.0, 20.0, 30.0, 40.0}};
  quickshot::MoveState state{rectangle, {20.0, 30.0}, {0.0, 0.0, 100.0, 80.0}};

  state.update({200.0, 200.0});
  QCOMPARE(rectangle.boundingRect(), QRectF(70.0, 40.0, 30.0, 40.0));

  state.update({-100.0, -100.0});
  QCOMPARE(rectangle.boundingRect(), QRectF(0.0, 0.0, 30.0, 40.0));
}

void DragStateTest::resizeStateUpdatesTheActiveHandle() {
  quickshot::Rectangle rectangle{QRectF{10.0, 20.0, 30.0, 40.0}};
  quickshot::ResizeState state{
      rectangle, quickshot::HandlePosition::BottomRight, {0.0, 0.0, 100.0, 80.0}};

  QCOMPARE(state.activeHandle(), quickshot::HandlePosition::BottomRight);
  QCOMPARE(state.completionButton(), Qt::LeftButton);

  state.update({80.0, 70.0});

  QCOMPARE(rectangle.boundingRect(), QRectF(10.0, 20.0, 70.0, 50.0));
}

void DragStateTest::rotateStateTracksTheMouseAngle() {
  quickshot::Rectangle rectangle{QRectF{10.0, 20.0, 60.0, 40.0}};
  quickshot::RotateState state{rectangle, quickshot::HandlePosition::Right, {70.0, 40.0}};

  QCOMPARE(state.activeHandle(), quickshot::HandlePosition::Right);
  QCOMPARE(state.completionButton(), Qt::RightButton);

  state.update({40.0, 70.0});

  QVERIFY(qAbs(rectangle.rotationDegrees() - 90.0) < 0.0001);
}

QTEST_MAIN(DragStateTest)

#include "drag_state_test.moc"
