#include "quickshot/main_window.hpp"
#include "quickshot/qdrawwidget.hpp"

#include <QColor>
#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QPushButton>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTest>

class MainWindowTest final : public QObject {
  Q_OBJECT

private slots:
  void hasExpectedTitle();
  void hasUsefulDefaultSize();
  void providesImageControls();
  void drawsImageAtOriginalSize();
  void showsScrollBarsForLargeImage();
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

  QVERIFY(openButton != nullptr);
  QCOMPARE(openButton->text(), QStringLiteral("Open"));
  QVERIFY(drawWidget != nullptr);
  QCOMPARE(window.centralWidget(), drawWidget);
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
