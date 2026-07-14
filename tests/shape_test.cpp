#include "quickshot/ellipse.hpp"
#include "quickshot/rectangle.hpp"
#include "quickshot/roi_exporter.hpp"
#include "quickshot/size_handle.hpp"

#include <QColor>
#include <QImage>
#include <QImageReader>
#include <QPointF>
#include <QRectF>
#include <QTemporaryDir>
#include <QTest>
#include <QTransform>

class ShapeTest final : public QObject {
  Q_OBJECT

private slots:
  void rectangleProvidesEightHandles();
  void ellipseProvidesCardinalHandles();
  void movesAndTransformsGeometry();
  void sizeHandleProvidesHitAreaAndCursor();
  void extractsRectangleRoi();
  void extractsTransparentEllipseRoi();
  void savesRoiAsPng();
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

void ShapeTest::extractsRectangleRoi() {
  QImage image{{20, 20}, QImage::Format_RGB32};
  image.fill(Qt::red);
  const quickshot::Rectangle rectangle{QRectF{2.0, 3.0, 5.0, 4.0}};

  const QImage roi = quickshot::extractRoi(image, rectangle);

  QCOMPARE(roi.size(), QSize(5, 4));
  QCOMPARE(roi.pixelColor(0, 0), QColor(Qt::red));
  QCOMPARE(roi.pixelColor(4, 3), QColor(Qt::red));
}

void ShapeTest::extractsTransparentEllipseRoi() {
  QImage image{{20, 20}, QImage::Format_RGB32};
  image.fill(Qt::red);
  const quickshot::Ellipse ellipse{QRectF{2.0, 2.0, 10.0, 10.0}};

  const QImage roi = quickshot::extractRoi(image, ellipse);

  QCOMPARE(roi.size(), QSize(10, 10));
  QCOMPARE(roi.pixelColor(0, 0).alpha(), 0);
  QCOMPARE(roi.pixelColor(5, 5), QColor(Qt::red));
}

void ShapeTest::savesRoiAsPng() {
  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());

  QImage image{{20, 20}, QImage::Format_RGB32};
  image.fill(Qt::blue);
  const quickshot::Rectangle rectangle{QRectF{2.0, 3.0, 5.0, 4.0}};
  const QString fileName = temporaryDirectory.filePath("roi.png");
  QString errorMessage;

  QVERIFY(quickshot::saveRoiPng(image, rectangle, fileName, &errorMessage));
  QVERIFY(errorMessage.isEmpty());

  QImageReader reader{fileName};
  QCOMPARE(reader.format(), QByteArray("png"));
  QCOMPARE(reader.read().size(), QSize(5, 4));
}

QTEST_APPLESS_MAIN(ShapeTest)

#include "shape_test.moc"
