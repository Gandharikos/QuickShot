#include "quickshot/ellipse.hpp"
#include "quickshot/rectangle.hpp"
#include "quickshot/size_handle.hpp"

#include <QPointF>
#include <QRectF>
#include <QTest>
#include <QTransform>

class ShapeTest final : public QObject {
  Q_OBJECT

private slots:
  void rectangleProvidesEightHandles();
  void ellipseProvidesCardinalHandles();
  void movesAndTransformsGeometry();
  void sizeHandleProvidesHitAreaAndCursor();
};

void ShapeTest::rectangleProvidesEightHandles() {
  const quickshot::Rectangle rectangle{QRectF{10.0, 20.0, 30.0, 40.0}};

  QCOMPARE(rectangle.boundingRect(), QRectF(10.0, 20.0, 30.0, 40.0));
  QVERIFY(rectangle.contains({20.0, 30.0}));
  QVERIFY(!rectangle.contains({5.0, 5.0}));
  QCOMPARE(rectangle.handles().size(), std::size_t{8});
  QCOMPARE(rectangle.handles().front().position(), quickshot::HandlePosition::TopLeft);
  QCOMPARE(rectangle.handles().back().position(), quickshot::HandlePosition::Left);
}

void ShapeTest::ellipseProvidesCardinalHandles() {
  const quickshot::Ellipse ellipse{QRectF{10.0, 20.0, 30.0, 40.0}};

  QCOMPARE(ellipse.boundingRect(), QRectF(10.0, 20.0, 30.0, 40.0));
  QVERIFY(ellipse.contains(ellipse.boundingRect().center()));
  QVERIFY(!ellipse.contains(ellipse.boundingRect().topLeft()));
  QCOMPARE(ellipse.handles().size(), std::size_t{4});
  QCOMPARE(ellipse.handles()[0].position(), quickshot::HandlePosition::Top);
  QCOMPARE(ellipse.handles()[1].position(), quickshot::HandlePosition::Right);
  QCOMPARE(ellipse.handles()[2].position(), quickshot::HandlePosition::Bottom);
  QCOMPARE(ellipse.handles()[3].position(), quickshot::HandlePosition::Left);
}

void ShapeTest::movesAndTransformsGeometry() {
  quickshot::Rectangle rectangle{QRectF{10.0, 20.0, 30.0, 40.0}};
  rectangle.moveBy({5.0, -10.0});
  QCOMPARE(rectangle.boundingRect(), QRectF(15.0, 10.0, 30.0, 40.0));

  QTransform transformation;
  transformation.scale(2.0, 3.0);
  rectangle.transform(transformation);
  QCOMPARE(rectangle.boundingRect(), QRectF(30.0, 30.0, 60.0, 120.0));
}

void ShapeTest::sizeHandleProvidesHitAreaAndCursor() {
  const QRectF bounds{10.0, 20.0, 30.0, 40.0};
  const quickshot::SizeHandle topHandle{quickshot::HandlePosition::Top};
  const quickshot::SizeHandle bottomRightHandle{quickshot::HandlePosition::BottomRight};

  QCOMPARE(topHandle.center(bounds), QPointF(25.0, 20.0));
  QCOMPARE(topHandle.hitRect(bounds, 8.0), QRectF(21.0, 16.0, 8.0, 8.0));
  QCOMPARE(topHandle.cursorShape(), Qt::SizeVerCursor);
  QCOMPARE(bottomRightHandle.center(bounds), QPointF(40.0, 60.0));
  QCOMPARE(bottomRightHandle.cursorShape(), Qt::SizeFDiagCursor);
}

QTEST_APPLESS_MAIN(ShapeTest)

#include "shape_test.moc"
