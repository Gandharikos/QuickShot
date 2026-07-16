#include "quickshot/roi_exporter.hpp"
#include "quickshot/shapes/bezier_curve.hpp"
#include "quickshot/shapes/circle.hpp"
#include "quickshot/shapes/ellipse.hpp"
#include "quickshot/shapes/polygon.hpp"
#include "quickshot/shapes/rectangle.hpp"
#include "quickshot/shapes/shape_handle.hpp"

#include <QColor>
#include <QImage>
#include <QImageReader>
#include <QLineF>
#include <QPointF>
#include <QRectF>
#include <QTemporaryDir>
#include <QTest>
#include <QTransform>
#include <stdexcept>

class ShapeTest final : public QObject {
  Q_OBJECT

private slots:
  void factoryCreatesConcreteShapes();
  void rectangleProvidesEightHandles();
  void ellipseProvidesEightHandles();
  void circlePreservesItsAspectRatio();
  void polygonProvidesVertexHandlesAndAClosedPath();
  void bezierCurveProvidesAutomaticClosedCurve();
  void polygonGeometryMementoRestoresVertices();
  void clonePreservesConcreteShape();
  void rotationTransformsPathAndHandles();
  void movesAndTransformsGeometry();
  void shapeHandlesExposeIdsCentersAndCursors();
  void geometryMementoRestoresResize();
  void extractsRectangleRoi();
  void validatesRoiBoundsAgainstImage();
  void extractsTransparentEllipseRoi();
  void extractsTransparentPolygonRoi();
  void extractsTransparentBezierRoi();
  void extractsRotatedRoi();
  void savesRoiAsPng();
};

void ShapeTest::factoryCreatesConcreteShapes() {
  const QRectF bounds{10.0, 20.0, 30.0, 40.0};

  const std::unique_ptr<quickshot::Shape> rectangle =
      quickshot::Shape::make(quickshot::ShapeType::Rectangle, bounds);
  const std::unique_ptr<quickshot::Shape> ellipse =
      quickshot::Shape::make(quickshot::ShapeType::Ellipse, bounds);
  const std::unique_ptr<quickshot::Shape> circle =
      quickshot::Shape::make(quickshot::ShapeType::Circle, bounds);
  const std::unique_ptr<quickshot::Shape> polygon =
      quickshot::Shape::make(quickshot::ShapeType::Polygon, bounds);
  const std::unique_ptr<quickshot::Shape> bezierCurve =
      quickshot::Shape::make(quickshot::ShapeType::BezierCurve, bounds);

  QVERIFY(dynamic_cast<const quickshot::Rectangle*>(rectangle.get()) != nullptr);
  QVERIFY(dynamic_cast<const quickshot::Ellipse*>(ellipse.get()) != nullptr);
  QVERIFY(dynamic_cast<const quickshot::Circle*>(circle.get()) != nullptr);
  QVERIFY(dynamic_cast<const quickshot::Ellipse*>(circle.get()) != nullptr);
  QVERIFY(dynamic_cast<const quickshot::Polygon*>(polygon.get()) != nullptr);
  QVERIFY(dynamic_cast<const quickshot::BezierCurve*>(bezierCurve.get()) != nullptr);
  QCOMPARE(rectangle->boundingRect(), bounds);
  QCOMPARE(ellipse->boundingRect(), bounds);
  QCOMPARE(polygon->boundingRect(), bounds);
  QCOMPARE(bezierCurve->boundingRect(), bounds);
  QVERIFY_EXCEPTION_THROWN(
      static_cast<void>(quickshot::Shape::make(quickshot::ShapeType::Count, bounds)),
      std::invalid_argument);
}

void ShapeTest::circlePreservesItsAspectRatio() {
  quickshot::Circle circle{QRectF{10.0, 20.0, 30.0, 40.0}};
  QCOMPARE(circle.boundingRect(), QRectF(10.0, 25.0, 30.0, 30.0));
  QCOMPARE(circle.handles().size(), std::size_t{8});

  circle.updateCreation({20.0, 20.0}, {0.0, 0.0, 100.0, 100.0}, {70.0, 50.0});
  QCOMPARE(circle.boundingRect(), QRectF(20.0, 20.0, 50.0, 50.0));

  const std::unique_ptr<quickshot::ShapeGeometry> geometry = circle.captureGeometry();
  circle.resize(*geometry, quickshot::ShapeHandle{quickshot::HandlePosition::BottomRight},
                {80.0, 60.0}, {0.0, 0.0, 100.0, 100.0});
  QCOMPARE(circle.boundingRect().width(), circle.boundingRect().height());
  QCOMPARE(circle.handleCenter(quickshot::ShapeHandle{quickshot::HandlePosition::TopLeft}),
           QPointF(20.0, 20.0));
}

void ShapeTest::polygonProvidesVertexHandlesAndAClosedPath() {
  quickshot::Polygon polygon{QRectF{10.0, 10.0, 0.0, 0.0}};
  polygon.appendPoint({70.0, 10.0});
  polygon.appendPoint({40.0, 70.0});
  polygon.setPreviewPoint({90.0, 90.0});

  QCOMPARE(polygon.pointCount(), qsizetype{3});
  QVERIFY(!polygon.isCreationComplete());
  polygon.finishCreation();

  QVERIFY(polygon.isCreationComplete());
  QCOMPARE(polygon.handles().size(), std::size_t{3});
  QCOMPARE(polygon.handleCenter(polygon.handles().back()), QPointF(40.0, 70.0));
  QVERIFY(polygon.contains({40.0, 30.0}));
  QVERIFY(!polygon.contains({90.0, 90.0}));
}

void ShapeTest::bezierCurveProvidesAutomaticClosedCurve() {
  quickshot::BezierCurve curve{QRectF{10.0, 10.0, 0.0, 0.0}};
  curve.appendPoint({70.0, 10.0});
  curve.appendPoint({70.0, 70.0});
  curve.appendPoint({10.0, 70.0});
  curve.setPreviewPoint({40.0, 90.0});

  QVERIFY(!curve.isCreationComplete());
  QVERIFY(curve.path().elementCount() > curve.pointCount());
  QVERIFY(curve.path().elementAt(1).isCurveTo());

  curve.finishCreation();

  QVERIFY(curve.isCreationComplete());
  QCOMPARE(curve.pointCount(), qsizetype{4});
  QCOMPARE(curve.handles().size(), std::size_t{4});
  QVERIFY(curve.path().elementAt(1).isCurveTo());
  QVERIFY(curve.contains({40.0, 40.0}));
  QVERIFY(!curve.contains({90.0, 90.0}));
}

void ShapeTest::polygonGeometryMementoRestoresVertices() {
  quickshot::Polygon polygon{{{10.0, 10.0}, {70.0, 10.0}, {40.0, 70.0}}};
  polygon.setRotationDegrees(30.0);
  const QPointF initialHandleCenter = polygon.handleCenter(polygon.handles()[1]);
  const std::unique_ptr<quickshot::ShapeGeometry> geometry = polygon.captureGeometry();

  polygon.resize(*geometry, polygon.handles()[1], {90.0, 20.0}, {0.0, 0.0, 100.0, 100.0});
  const QLineF handleError{polygon.handleCenter(polygon.handles()[1]), QPointF{90.0, 20.0}};
  QVERIFY(handleError.length() < 0.0001);

  polygon.restoreGeometry(*geometry);
  QCOMPARE(polygon.handleCenter(polygon.handles()[1]), initialHandleCenter);
}

void ShapeTest::rectangleProvidesEightHandles() {
  const quickshot::Rectangle rectangle{QRectF{10.0, 20.0, 30.0, 40.0}};

  QCOMPARE(rectangle.boundingRect(), QRectF(10.0, 20.0, 30.0, 40.0));
  QVERIFY(rectangle.contains({20.0, 30.0}));
  QVERIFY(!rectangle.contains({5.0, 5.0}));
  QCOMPARE(rectangle.handles().size(), std::size_t{8});
  QCOMPARE(rectangle.handles().front().id(),
           quickshot::ShapeHandle{quickshot::HandlePosition::TopLeft}.id());
  QCOMPARE(rectangle.handles().back().id(),
           quickshot::ShapeHandle{quickshot::HandlePosition::Left}.id());
}

void ShapeTest::ellipseProvidesEightHandles() {
  const quickshot::Ellipse ellipse{QRectF{10.0, 20.0, 30.0, 40.0}};

  QCOMPARE(ellipse.boundingRect(), QRectF(10.0, 20.0, 30.0, 40.0));
  QVERIFY(ellipse.contains(ellipse.boundingRect().center()));
  QVERIFY(!ellipse.contains(ellipse.boundingRect().topLeft()));
  QCOMPARE(ellipse.handles().size(), std::size_t{8});
  QCOMPARE(ellipse.handles().front().id(),
           quickshot::ShapeHandle{quickshot::HandlePosition::TopLeft}.id());
  QCOMPARE(ellipse.handles().back().id(),
           quickshot::ShapeHandle{quickshot::HandlePosition::Left}.id());
}

void ShapeTest::clonePreservesConcreteShape() {
  quickshot::Rectangle rectangle{QRectF{10.0, 20.0, 80.0, 40.0}};
  const quickshot::Ellipse ellipse{QRectF{5.0, 6.0, 30.0, 20.0}};
  const quickshot::Circle circle{QRectF{4.0, 8.0, 20.0, 20.0}};
  const quickshot::Polygon polygon{{{10.0, 10.0}, {70.0, 10.0}, {40.0, 70.0}}};
  const quickshot::BezierCurve bezierCurve{
      {{10.0, 10.0}, {70.0, 10.0}, {70.0, 70.0}, {10.0, 70.0}}};
  rectangle.setRotationDegrees(42.0);

  const std::unique_ptr<quickshot::Shape> rectangleClone = rectangle.clone();
  const std::unique_ptr<quickshot::Shape> ellipseClone = ellipse.clone();
  const std::unique_ptr<quickshot::Shape> circleClone = circle.clone();
  const std::unique_ptr<quickshot::Shape> polygonClone = polygon.clone();
  const std::unique_ptr<quickshot::Shape> bezierCurveClone = bezierCurve.clone();

  QVERIFY(dynamic_cast<const quickshot::Rectangle*>(rectangleClone.get()) != nullptr);
  QVERIFY(dynamic_cast<const quickshot::Ellipse*>(ellipseClone.get()) != nullptr);
  QVERIFY(dynamic_cast<const quickshot::Circle*>(circleClone.get()) != nullptr);
  QVERIFY(dynamic_cast<const quickshot::Polygon*>(polygonClone.get()) != nullptr);
  QVERIFY(dynamic_cast<const quickshot::BezierCurve*>(bezierCurveClone.get()) != nullptr);
  QCOMPARE(rectangleClone->boundingRect(), rectangle.boundingRect());
  QCOMPARE(ellipseClone->boundingRect(), ellipse.boundingRect());
  QCOMPARE(rectangleClone->rotationDegrees(), 42.0);
}

void ShapeTest::rotationTransformsPathAndHandles() {
  quickshot::Rectangle rectangle{QRectF{10.0, 20.0, 30.0, 10.0}};
  rectangle.setRotationDegrees(90.0);

  QCOMPARE(rectangle.rotationDegrees(), 90.0);
  QCOMPARE(rectangle.path().boundingRect(), QRectF(20.0, 10.0, 10.0, 30.0));
  QCOMPARE(rectangle.handleCenter(rectangle.handles().front()), QPointF(30.0, 10.0));
  QCOMPARE(rectangle.mapFromImage({30.0, 10.0}), QPointF(10.0, 20.0));
  QVERIFY(rectangle.contains(rectangle.boundingRect().center()));
}

void ShapeTest::movesAndTransformsGeometry() {
  quickshot::Rectangle rectangle{QRectF{10.0, 20.0, 30.0, 40.0}};
  rectangle.moveBy({5.0, -10.0});
  QCOMPARE(rectangle.boundingRect(), QRectF(15.0, 10.0, 30.0, 40.0));

  QTransform transformation;
  transformation.scale(2.0, 3.0);
  rectangle.transform(transformation);
  QCOMPARE(rectangle.boundingRect(), QRectF(30.0, 30.0, 60.0, 120.0));

  quickshot::Polygon polygon{{{10.0, 10.0}, {70.0, 10.0}, {40.0, 70.0}}};
  polygon.setBoundingRect({20.0, 30.0, 120.0, 120.0});
  QCOMPARE(polygon.boundingRect(), QRectF(20.0, 30.0, 120.0, 120.0));
  QCOMPARE(polygon.handleCenter(polygon.handles().back()), QPointF(80.0, 150.0));
}

void ShapeTest::shapeHandlesExposeIdsCentersAndCursors() {
  const quickshot::Rectangle rectangle{QRectF{10.0, 20.0, 30.0, 40.0}};
  const quickshot::ShapeHandle topHandle{quickshot::HandlePosition::Top};
  const quickshot::ShapeHandle bottomRightHandle{quickshot::HandlePosition::BottomRight};
  const quickshot::ShapeHandle polygonVertexHandle{42, Qt::CrossCursor};

  QCOMPARE(rectangle.handleCenter(topHandle), QPointF(25.0, 20.0));
  QCOMPARE(topHandle.cursorShape(), Qt::SizeVerCursor);
  QCOMPARE(rectangle.handleCenter(bottomRightHandle), QPointF(40.0, 60.0));
  QCOMPARE(bottomRightHandle.cursorShape(), Qt::SizeFDiagCursor);
  QCOMPARE(polygonVertexHandle.id(), quickshot::ShapeHandle::Id{42});
  QCOMPARE(polygonVertexHandle.cursorShape(), Qt::CrossCursor);
}

void ShapeTest::geometryMementoRestoresResize() {
  quickshot::Rectangle rectangle{QRectF{10.0, 20.0, 30.0, 40.0}};
  rectangle.setRotationDegrees(15.0);
  const std::unique_ptr<quickshot::ShapeGeometry> initialGeometry = rectangle.captureGeometry();

  rectangle.resize(*initialGeometry, quickshot::ShapeHandle{quickshot::HandlePosition::BottomRight},
                   {80.0, 70.0}, {0.0, 0.0, 100.0, 80.0});
  QVERIFY(rectangle.boundingRect() != QRectF(10.0, 20.0, 30.0, 40.0));

  rectangle.restoreGeometry(*initialGeometry);
  QCOMPARE(rectangle.boundingRect(), QRectF(10.0, 20.0, 30.0, 40.0));
  QCOMPARE(rectangle.rotationDegrees(), 15.0);
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

void ShapeTest::validatesRoiBoundsAgainstImage() {
  const QImage image{{20, 20}, QImage::Format_RGB32};
  const quickshot::Rectangle inside{QRectF{2.0, 3.0, 5.0, 4.0}};
  const quickshot::Rectangle touchingEdge{QRectF{15.0, 15.0, 5.0, 5.0}};
  const quickshot::Rectangle partiallyOutside{QRectF{15.0, 15.0, 6.0, 5.0}};
  const quickshot::Rectangle outside{QRectF{22.0, 22.0, 5.0, 5.0}};

  QVERIFY(quickshot::isRoiWithinImage(image, inside));
  QVERIFY(quickshot::isRoiWithinImage(image, touchingEdge));
  QVERIFY(!quickshot::isRoiWithinImage(image, partiallyOutside));
  QVERIFY(!quickshot::isRoiWithinImage(image, outside));
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

void ShapeTest::extractsTransparentPolygonRoi() {
  QImage image{{20, 20}, QImage::Format_RGB32};
  image.fill(Qt::red);
  const quickshot::Polygon polygon{{{2.0, 2.0}, {12.0, 2.0}, {2.0, 12.0}}};

  const QImage roi = quickshot::extractRoi(image, polygon);

  QCOMPARE(roi.size(), QSize(10, 10));
  QCOMPARE(roi.pixelColor(1, 1), QColor(Qt::red));
  QCOMPARE(roi.pixelColor(9, 9).alpha(), 0);
}

void ShapeTest::extractsTransparentBezierRoi() {
  QImage image{{20, 20}, QImage::Format_RGB32};
  image.fill(Qt::red);
  const quickshot::BezierCurve curve{{{3.0, 3.0}, {13.0, 3.0}, {13.0, 13.0}, {3.0, 13.0}}};

  const QImage roi = quickshot::extractRoi(image, curve);

  QVERIFY(!roi.isNull());
  QCOMPARE(roi.pixelColor(roi.width() / 2, roi.height() / 2), QColor(Qt::red));
  QCOMPARE(roi.pixelColor(0, 0).alpha(), 0);
}

void ShapeTest::extractsRotatedRoi() {
  QImage image{{30, 30}, QImage::Format_RGB32};
  image.fill(Qt::red);
  quickshot::Rectangle rectangle{QRectF{8.0, 10.0, 12.0, 6.0}};
  rectangle.setRotationDegrees(45.0);

  const QImage roi = quickshot::extractRoi(image, rectangle);

  QVERIFY(!roi.isNull());
  QCOMPARE(roi.pixelColor(0, 0).alpha(), 0);
  QCOMPARE(roi.pixelColor(roi.width() / 2, roi.height() / 2), QColor(Qt::red));
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
