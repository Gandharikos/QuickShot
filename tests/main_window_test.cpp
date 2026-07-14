#include "quickshot/main_window.hpp"
#include "quickshot/qdrawwidget.hpp"
#include "quickshot/shape.hpp"

#include <QAction>
#include <QColor>
#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QPointF>
#include <QPushButton>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTest>
#include <QToolBar>
#include <QWheelEvent>

namespace {

void sendControlWheel(quickshot::QDrawWidget& drawWidget, int angleDelta,
                      const QPointF& position = {30.0, 20.0}) {
  const QPoint globalPosition = drawWidget.viewport()->mapToGlobal(position.toPoint());
  QWheelEvent event{position,     globalPosition,      QPoint{},          QPoint{0, angleDelta},
                    Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false};
  QCoreApplication::sendEvent(drawWidget.viewport(), &event);
  QCoreApplication::processEvents();
}

void drag(QWidget* widget, const QPoint& start, const QPoint& end) {
  QTest::mousePress(widget, Qt::LeftButton, Qt::NoModifier, start);
  QTest::mouseMove(widget, end);
  QTest::mouseRelease(widget, Qt::LeftButton, Qt::NoModifier, end);
  QCoreApplication::processEvents();
}

} // namespace

class MainWindowTest final : public QObject {
  Q_OBJECT

private slots:
  void hasExpectedTitle();
  void hasUsefulDefaultSize();
  void providesImageControls();
  void enablesAndRunsRotationActions();
  void createsMovesAndResizesShapes();
  void createsShapesInImageCoordinates();
  void drawsImageAtOriginalSize();
  void showsScrollBarsForLargeImage();
  void zoomsFromImageTopLeft();
  void clampsZoomAndResetsForNewImage();
  void rejectsNonImageFiles();
};

void MainWindowTest::hasExpectedTitle() {
  const quickshot::MainWindow window;
  QCOMPARE(window.windowTitle(), QStringLiteral("Quickshot"));
}

void MainWindowTest::hasUsefulDefaultSize() {
  const quickshot::MainWindow window;
  QVERIFY(window.width() >= 640);
  QVERIFY(window.height() >= 360);
}

void MainWindowTest::providesImageControls() {
  const quickshot::MainWindow window;
  const auto* openButton = window.findChild<QPushButton*>("openButton");
  const auto* drawWidget = window.findChild<quickshot::QDrawWidget*>("drawWidget");
  const auto* toolbar = window.findChild<QToolBar*>("mainToolBar");
  const auto* rotateLeftAction = window.findChild<QAction*>("rotateLeftAction");
  const auto* rotateRightAction = window.findChild<QAction*>("rotateRightAction");
  const auto* rectangleAction = window.findChild<QAction*>("rectangleAction");
  const auto* ellipseAction = window.findChild<QAction*>("ellipseAction");

  QVERIFY(openButton != nullptr);
  QCOMPARE(openButton->text(), QStringLiteral("Open"));
  QVERIFY(drawWidget != nullptr);
  QCOMPARE(window.centralWidget(), drawWidget);
  QVERIFY(toolbar != nullptr);
  QVERIFY(rotateLeftAction != nullptr);
  QVERIFY(rotateRightAction != nullptr);
  QVERIFY(rectangleAction != nullptr);
  QVERIFY(ellipseAction != nullptr);
  QCOMPARE(rotateLeftAction->text(), QStringLiteral("Rotate Left"));
  QCOMPARE(rotateRightAction->text(), QStringLiteral("Rotate Right"));
  QVERIFY(!rotateLeftAction->icon().isNull());
  QVERIFY(!rotateRightAction->icon().isNull());
  QVERIFY(!rotateLeftAction->isEnabled());
  QVERIFY(!rotateRightAction->isEnabled());
  QVERIFY(rectangleAction->isCheckable());
  QVERIFY(ellipseAction->isCheckable());
  QVERIFY(!rectangleAction->isEnabled());
  QVERIFY(!ellipseAction->isEnabled());

  const QList<QAction*> toolbarActions = toolbar->actions();
  QCOMPARE(toolbarActions.size(), qsizetype{7});
  QVERIFY(toolbarActions.at(1)->isSeparator());
  QCOMPARE(toolbarActions.at(2), rotateLeftAction);
  QCOMPARE(toolbarActions.at(3), rotateRightAction);
  QVERIFY(toolbarActions.at(4)->isSeparator());
  QCOMPARE(toolbarActions.at(5), rectangleAction);
  QCOMPARE(toolbarActions.at(6), ellipseAction);
}

void MainWindowTest::enablesAndRunsRotationActions() {
  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());

  QImage sourceImage({400, 40}, QImage::Format_RGB32);
  sourceImage.fill(Qt::red);
  const QString imagePath = temporaryDirectory.filePath("rotate.png");
  QVERIFY(sourceImage.save(imagePath));

  quickshot::MainWindow window;
  auto* drawWidget = window.findChild<quickshot::QDrawWidget*>("drawWidget");
  auto* rotateLeftAction = window.findChild<QAction*>("rotateLeftAction");
  auto* rotateRightAction = window.findChild<QAction*>("rotateRightAction");
  auto* rectangleAction = window.findChild<QAction*>("rectangleAction");
  auto* ellipseAction = window.findChild<QAction*>("ellipseAction");
  QVERIFY(drawWidget != nullptr);
  QVERIFY(rotateLeftAction != nullptr);
  QVERIFY(rotateRightAction != nullptr);
  QVERIFY(rectangleAction != nullptr);
  QVERIFY(ellipseAction != nullptr);

  window.resize(180, 180);
  window.show();
  QCoreApplication::processEvents();
  QVERIFY(drawWidget->loadImage(imagePath));

  QVERIFY(rotateLeftAction->isEnabled());
  QVERIFY(rotateRightAction->isEnabled());
  QVERIFY(rectangleAction->isEnabled());
  QVERIFY(ellipseAction->isEnabled());
  QVERIFY(drawWidget->horizontalScrollBar()->maximum() > 0);
  QCOMPARE(drawWidget->verticalScrollBar()->maximum(), 0);

  rotateRightAction->trigger();
  QCOMPARE(drawWidget->horizontalScrollBar()->maximum(), 0);
  QVERIFY(drawWidget->verticalScrollBar()->maximum() > 0);

  rotateLeftAction->trigger();
  QVERIFY(drawWidget->horizontalScrollBar()->maximum() > 0);
  QCOMPARE(drawWidget->verticalScrollBar()->maximum(), 0);
}

void MainWindowTest::createsMovesAndResizesShapes() {
  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());

  QImage sourceImage({200, 150}, QImage::Format_RGB32);
  sourceImage.fill(Qt::black);
  const QString imagePath = temporaryDirectory.filePath("shapes.png");
  QVERIFY(sourceImage.save(imagePath));

  quickshot::QDrawWidget drawWidget;
  drawWidget.resize(220, 180);
  QVERIFY(drawWidget.loadImage(imagePath));
  drawWidget.show();
  QCoreApplication::processEvents();

  drawWidget.setRectangleCreationMode(true);
  drag(drawWidget.viewport(), {20, 20}, {80, 60});
  QCOMPARE(drawWidget.shapeCount(), qsizetype{1});
  QCOMPARE(drawWidget.shapeAt(0)->boundingRect(), QRectF(20.0, 20.0, 60.0, 40.0));
  QCOMPARE(drawWidget.shapeAt(0)->handles().size(), std::size_t{8});

  QTest::mouseMove(drawWidget.viewport(), {80, 60});
  QCOMPARE(drawWidget.viewport()->cursor().shape(), Qt::SizeFDiagCursor);
  drag(drawWidget.viewport(), {80, 60}, {100, 90});
  QCOMPARE(drawWidget.shapeAt(0)->boundingRect(), QRectF(20.0, 20.0, 80.0, 70.0));

  QTest::mouseMove(drawWidget.viewport(), {50, 50});
  QCOMPARE(drawWidget.viewport()->cursor().shape(), Qt::CrossCursor);
  drag(drawWidget.viewport(), {50, 50}, {60, 60});
  QCOMPARE(drawWidget.shapeAt(0)->boundingRect(), QRectF(30.0, 30.0, 80.0, 70.0));

  drawWidget.setRectangleCreationMode(false);
  drawWidget.setEllipseCreationMode(true);
  drag(drawWidget.viewport(), {120, 20}, {180, 80});
  QCOMPARE(drawWidget.shapeCount(), qsizetype{2});
  QCOMPARE(drawWidget.shapeAt(1)->boundingRect(), QRectF(120.0, 20.0, 60.0, 60.0));
  QCOMPARE(drawWidget.shapeAt(1)->handles().size(), std::size_t{4});

  QTest::mouseMove(drawWidget.viewport(), {150, 20});
  QCOMPARE(drawWidget.viewport()->cursor().shape(), Qt::SizeVerCursor);

  drawWidget.rotateRight();
  QCOMPARE(drawWidget.shapeAt(0)->boundingRect().size(), QSizeF(70.0, 80.0));
}

void MainWindowTest::createsShapesInImageCoordinates() {
  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());

  QImage sourceImage({400, 300}, QImage::Format_RGB32);
  sourceImage.fill(Qt::black);
  const QString imagePath = temporaryDirectory.filePath("coordinates.png");
  QVERIFY(sourceImage.save(imagePath));

  quickshot::QDrawWidget drawWidget;
  drawWidget.resize(100, 80);
  QVERIFY(drawWidget.loadImage(imagePath));
  drawWidget.show();
  QCoreApplication::processEvents();
  sendControlWheel(drawWidget, 120);
  drawWidget.horizontalScrollBar()->setValue(22);
  drawWidget.verticalScrollBar()->setValue(11);

  drawWidget.setRectangleCreationMode(true);
  drag(drawWidget.viewport(), {11, 11}, {33, 33});

  QCOMPARE(drawWidget.shapeCount(), qsizetype{1});
  const QRectF bounds = drawWidget.shapeAt(0)->boundingRect();
  QVERIFY(qAbs(bounds.x() - 30.0) < 0.0001);
  QVERIFY(qAbs(bounds.y() - 20.0) < 0.0001);
  QVERIFY(qAbs(bounds.width() - 20.0) < 0.0001);
  QVERIFY(qAbs(bounds.height() - 20.0) < 0.0001);
}

void MainWindowTest::drawsImageAtOriginalSize() {
  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());

  QImage sourceImage({40, 20}, QImage::Format_RGB32);
  sourceImage.fill(Qt::red);
  const QString imagePath = temporaryDirectory.filePath("small.png");
  QVERIFY(sourceImage.save(imagePath));

  quickshot::QDrawWidget drawWidget;
  drawWidget.resize(100, 100);
  QVERIFY(drawWidget.loadImage(imagePath));
  QVERIFY(drawWidget.hasImage());
  drawWidget.show();
  QCoreApplication::processEvents();

  QImage renderedImage(drawWidget.viewport()->size(), QImage::Format_RGB32);
  renderedImage.fill(Qt::green);
  drawWidget.viewport()->render(&renderedImage);

  QCOMPARE(renderedImage.pixelColor(0, 0), QColor(Qt::red));
  QCOMPARE(renderedImage.pixelColor(39, 19), QColor(Qt::red));
  QCOMPARE(renderedImage.pixelColor(40, 19), QColor(Qt::black));
  QCOMPARE(renderedImage.pixelColor(39, 20), QColor(Qt::black));
  QCOMPARE(renderedImage.pixelColor(99, 99), QColor(Qt::black));
  QCOMPARE(drawWidget.horizontalScrollBar()->maximum(), 0);
  QCOMPARE(drawWidget.verticalScrollBar()->maximum(), 0);
}

void MainWindowTest::showsScrollBarsForLargeImage() {
  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());

  QImage sourceImage({200, 150}, QImage::Format_RGB32);
  sourceImage.fill(Qt::red);
  const QString imagePath = temporaryDirectory.filePath("large.png");
  QVERIFY(sourceImage.save(imagePath));

  quickshot::QDrawWidget drawWidget;
  drawWidget.resize(100, 80);
  QVERIFY(drawWidget.loadImage(imagePath));
  drawWidget.show();
  QCoreApplication::processEvents();

  QVERIFY(drawWidget.horizontalScrollBar()->maximum() > 0);
  QVERIFY(drawWidget.verticalScrollBar()->maximum() > 0);
  QCOMPARE(drawWidget.horizontalScrollBar()->maximum(),
           sourceImage.width() - drawWidget.viewport()->width());
  QCOMPARE(drawWidget.verticalScrollBar()->maximum(),
           sourceImage.height() - drawWidget.viewport()->height());
}

void MainWindowTest::zoomsFromImageTopLeft() {
  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());

  QImage sourceImage({400, 300}, QImage::Format_RGB32);
  sourceImage.fill(Qt::red);
  const QString imagePath = temporaryDirectory.filePath("zoom.png");
  QVERIFY(sourceImage.save(imagePath));

  quickshot::QDrawWidget drawWidget;
  drawWidget.resize(120, 100);
  QVERIFY(drawWidget.loadImage(imagePath));
  drawWidget.show();
  QCoreApplication::processEvents();

  drawWidget.horizontalScrollBar()->setValue(20);
  drawWidget.verticalScrollBar()->setValue(10);
  const int horizontalMaximum = drawWidget.horizontalScrollBar()->maximum();
  const int verticalMaximum = drawWidget.verticalScrollBar()->maximum();

  sendControlWheel(drawWidget, 120);

  QVERIFY(qAbs(drawWidget.zoomFactor() - 1.1) < 0.0001);
  QCOMPARE(drawWidget.horizontalScrollBar()->value(), 20);
  QCOMPARE(drawWidget.verticalScrollBar()->value(), 10);
  QVERIFY(drawWidget.horizontalScrollBar()->maximum() > horizontalMaximum);
  QVERIFY(drawWidget.verticalScrollBar()->maximum() > verticalMaximum);

  sendControlWheel(drawWidget, -120);

  QVERIFY(qAbs(drawWidget.zoomFactor() - 1.0) < 0.0001);
  QCOMPARE(drawWidget.horizontalScrollBar()->value(), 20);
  QCOMPARE(drawWidget.verticalScrollBar()->value(), 10);
}

void MainWindowTest::clampsZoomAndResetsForNewImage() {
  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());

  QImage sourceImage({200, 150}, QImage::Format_RGB32);
  sourceImage.fill(Qt::red);
  const QString imagePath = temporaryDirectory.filePath("limits.png");
  QVERIFY(sourceImage.save(imagePath));

  quickshot::QDrawWidget drawWidget;
  drawWidget.resize(100, 80);
  QVERIFY(drawWidget.loadImage(imagePath));
  drawWidget.show();
  QCoreApplication::processEvents();

  sendControlWheel(drawWidget, 12000);
  QCOMPARE(drawWidget.zoomFactor(), 8.0);

  drawWidget.horizontalScrollBar()->setValue(50);
  drawWidget.verticalScrollBar()->setValue(40);
  QVERIFY(drawWidget.loadImage(imagePath));
  QCOMPARE(drawWidget.zoomFactor(), 1.0);
  QCOMPARE(drawWidget.horizontalScrollBar()->value(), 0);
  QCOMPARE(drawWidget.verticalScrollBar()->value(), 0);

  sendControlWheel(drawWidget, -24000);
  QCOMPARE(drawWidget.zoomFactor(), 0.1);
}

void MainWindowTest::rejectsNonImageFiles() {
  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());

  QFile invalidImage(temporaryDirectory.filePath("invalid.png"));
  QVERIFY(invalidImage.open(QIODevice::WriteOnly));
  QCOMPARE(invalidImage.write("not an image"), qint64{12});
  invalidImage.close();

  quickshot::QDrawWidget drawWidget;
  QVERIFY(!drawWidget.loadImage(invalidImage.fileName()));
  QVERIFY(!drawWidget.hasImage());
}

QTEST_MAIN(MainWindowTest)

#include "main_window_test.moc"
