#include "quickshot/main_window.hpp"
#include "quickshot/qdrawwidget.hpp"
#include "quickshot/shapes/bezier_curve.hpp"
#include "quickshot/shapes/circle.hpp"
#include "quickshot/shapes/polygon.hpp"
#include "quickshot/shapes/shape.hpp"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QLineF>
#include <QListWidget>
#include <QMenu>
#include <QPointF>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
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

void sendWheel(QWidget& widget, int angleDelta) {
  const QPointF position{widget.rect().center()};
  const QPoint globalPosition = widget.mapToGlobal(position.toPoint());
  QWheelEvent event{position,     globalPosition, QPoint{},          QPoint{0, angleDelta},
                    Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false};
  QCoreApplication::sendEvent(&widget, &event);
  QCoreApplication::processEvents();
}

void drag(QWidget* widget, const QPoint& start, const QPoint& end) {
  QTest::mousePress(widget, Qt::LeftButton, Qt::NoModifier, start);
  QTest::mouseMove(widget, end);
  QTest::mouseRelease(widget, Qt::LeftButton, Qt::NoModifier, end);
  QCoreApplication::processEvents();
}

QImage renderViewport(const quickshot::QDrawWidget& drawWidget) {
  QImage renderedImage{drawWidget.viewport()->size(), QImage::Format_RGB32};
  renderedImage.fill(Qt::black);
  drawWidget.viewport()->render(&renderedImage);
  return renderedImage;
}

qsizetype colorPixelCount(const QImage& image, const QColor& color, const QRect& area) {
  qsizetype count = 0;
  const QRect boundedArea = area.intersected(image.rect());
  for (int y = boundedArea.top(); y <= boundedArea.bottom(); ++y) {
    for (int x = boundedArea.left(); x <= boundedArea.right(); ++x) {
      if (image.pixelColor(x, y) == color) {
        ++count;
      }
    }
  }
  return count;
}

bool hasColorNear(const QImage& image, const QPoint& point, const QColor& color) {
  constexpr int radius = 2;
  for (int y = point.y() - radius; y <= point.y() + radius; ++y) {
    for (int x = point.x() - radius; x <= point.x() + radius; ++x) {
      if (image.rect().contains(x, y) && image.pixelColor(x, y) == color) {
        return true;
      }
    }
  }
  return false;
}

bool triggerContextMenuAction(quickshot::QDrawWidget& drawWidget, const QPoint& position,
                              const char* actionName) {
  bool actionFound = false;
  QTimer::singleShot(0, [&actionFound, actionName]() {
    auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
    if (menu == nullptr) {
      return;
    }

    QAction* action = nullptr;
    for (QAction* candidate : menu->actions()) {
      if (candidate->objectName() == QLatin1String{actionName}) {
        action = candidate;
        break;
      }
    }
    if (action == nullptr) {
      menu->close();
      return;
    }

    actionFound = true;
    action->trigger();
    menu->close();
  });

  QContextMenuEvent event{QContextMenuEvent::Mouse, position,
                          drawWidget.viewport()->mapToGlobal(position)};
  QCoreApplication::sendEvent(drawWidget.viewport(), &event);
  QCoreApplication::processEvents();
  return actionFound;
}

bool triggerImageListContextMenuAction(QListWidget& imageList, int row, const char* actionName) {
  bool actionFound = false;
  QTimer::singleShot(0, [&actionFound, actionName]() {
    auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
    if (menu == nullptr) {
      return;
    }

    for (QAction* action : menu->actions()) {
      if (action->objectName() == QLatin1String{actionName}) {
        actionFound = true;
        action->trigger();
        break;
      }
    }
    menu->close();
  });

  QListWidgetItem* item = imageList.item(row);
  if (item == nullptr) {
    return false;
  }
  emit imageList.customContextMenuRequested(imageList.visualItemRect(item).center());
  QCoreApplication::processEvents();
  return actionFound;
}

struct BatchDialogSnapshot {
  bool actionFound = false;
  bool dialogFound = false;
  QStringList imageCells;
  QStringList statuses;
};

BatchDialogSnapshot inspectBatchContextAction(quickshot::QDrawWidget& drawWidget,
                                              const QPoint& position, const char* actionName,
                                              const QString& outputDirectory = {}) {
  BatchDialogSnapshot snapshot;
  QTimer::singleShot(0, [&snapshot, actionName, outputDirectory]() {
    auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
    if (menu == nullptr) {
      return;
    }

    QAction* action = nullptr;
    for (QAction* candidate : menu->actions()) {
      if (candidate->objectName() == QLatin1String{actionName}) {
        action = candidate;
        break;
      }
    }
    if (action == nullptr || !action->isEnabled()) {
      menu->close();
      return;
    }

    snapshot.actionFound = true;
    QTimer::singleShot(0, [&snapshot, outputDirectory]() {
      auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
      if (dialog == nullptr || dialog->objectName() != QLatin1String{"batchSaveDialog"}) {
        return;
      }
      snapshot.dialogFound = true;
      auto* table = dialog->findChild<QTableWidget*>("batchSaveTable");
      if (table != nullptr) {
        for (int row = 0; row < table->rowCount(); ++row) {
          snapshot.imageCells.push_back(table->item(row, 0)->text());
          snapshot.statuses.push_back(table->item(row, 1)->text());
        }
      }
      if (outputDirectory.isEmpty()) {
        dialog->reject();
        return;
      }
      auto* outputEdit = dialog->findChild<QLineEdit*>("batchOutputDirectoryEdit");
      if (outputEdit != nullptr) {
        outputEdit->setText(outputDirectory);
      }
      dialog->accept();
    });
    action->trigger();
    menu->close();
  });

  QContextMenuEvent event{QContextMenuEvent::Mouse, position,
                          drawWidget.viewport()->mapToGlobal(position)};
  QCoreApplication::sendEvent(drawWidget.viewport(), &event);
  QCoreApplication::processEvents();
  return snapshot;
}

} // namespace

class MainWindowTest final : public QObject {
  Q_OBJECT

private slots:
  void hasExpectedTitle();
  void hasUsefulDefaultSize();
  void providesImageControls();
  void remembersLastOpenDirectory();
  void synchronizesToolbarZoomControl();
  void showsImageCoordinatesInStatusBar();
  void enablesAndRunsRotationActions();
  void undoesAndRedoesDragOperations();
  void createsMovesAndResizesShapes();
  void createsCirclesAndPolygons();
  void createsBezierCurves();
  void switchesIndependentImageDocuments();
  void batchSaveUsesOnlyCurrentImageShapes();
  void hidesInactiveHandlesWhileResizing();
  void contextMenusCloneAndDeleteShapes();
  void resizesAndRotatesFromShapeHandles();
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
  const auto* undoAction = window.findChild<QAction*>("undoAction");
  const auto* redoAction = window.findChild<QAction*>("redoAction");
  const auto* rectangleAction = window.findChild<QAction*>("rectangleAction");
  const auto* ellipseAction = window.findChild<QAction*>("ellipseAction");
  const auto* circleAction = window.findChild<QAction*>("circleAction");
  const auto* polygonAction = window.findChild<QAction*>("polygonAction");
  const auto* bezierCurveAction = window.findChild<QAction*>("bezierCurveAction");
  const auto* coordinateLabel = window.findChild<QLabel*>("coordinateLabel");
  const auto* zoomFactorSpinBox = window.findChild<QDoubleSpinBox*>("zoomFactorSpinBox");

  QVERIFY(openButton != nullptr);
  QCOMPARE(openButton->text(), QStringLiteral("Open"));
  QVERIFY(drawWidget != nullptr);
  QCOMPARE(window.centralWidget(), drawWidget);
  QVERIFY(toolbar != nullptr);
  QCOMPARE(toolbar->toolButtonStyle(), Qt::ToolButtonIconOnly);
  QVERIFY(rotateLeftAction != nullptr);
  QVERIFY(rotateRightAction != nullptr);
  QVERIFY(undoAction != nullptr);
  QVERIFY(redoAction != nullptr);
  QVERIFY(rectangleAction != nullptr);
  QVERIFY(ellipseAction != nullptr);
  QVERIFY(circleAction != nullptr);
  QVERIFY(polygonAction != nullptr);
  QVERIFY(bezierCurveAction != nullptr);
  QVERIFY(coordinateLabel != nullptr);
  QCOMPARE(coordinateLabel->text(), QStringLiteral("X: —  Y: —"));
  QVERIFY(zoomFactorSpinBox != nullptr);
  QCOMPARE(rotateLeftAction->text(), QStringLiteral("Rotate Left"));
  QCOMPARE(rotateRightAction->text(), QStringLiteral("Rotate Right"));
  QVERIFY(!rotateLeftAction->icon().isNull());
  QVERIFY(!rotateRightAction->icon().isNull());
  QVERIFY(!rotateLeftAction->isEnabled());
  QVERIFY(!rotateRightAction->isEnabled());
  QVERIFY(!undoAction->isEnabled());
  QVERIFY(!redoAction->isEnabled());
  QVERIFY(!undoAction->icon().isNull());
  QVERIFY(!redoAction->icon().isNull());
  QCOMPARE(undoAction->shortcut(), QKeySequence{QKeySequence::Undo});
  QCOMPARE(redoAction->shortcut(), QKeySequence{QKeySequence::Redo});
  QVERIFY(rectangleAction->isCheckable());
  QVERIFY(ellipseAction->isCheckable());
  QVERIFY(circleAction->isCheckable());
  QVERIFY(polygonAction->isCheckable());
  QVERIFY(bezierCurveAction->isCheckable());
  QVERIFY(!rectangleAction->icon().isNull());
  QVERIFY(!ellipseAction->icon().isNull());
  QVERIFY(!circleAction->icon().isNull());
  QVERIFY(!polygonAction->icon().isNull());
  QVERIFY(!bezierCurveAction->icon().isNull());
  QVERIFY(!rectangleAction->isEnabled());
  QVERIFY(!ellipseAction->isEnabled());
  QVERIFY(!circleAction->isEnabled());
  QVERIFY(!polygonAction->isEnabled());
  QVERIFY(!bezierCurveAction->isEnabled());
  QVERIFY(!zoomFactorSpinBox->isEnabled());
  QCOMPARE(zoomFactorSpinBox->value(), 1.0);
  QCOMPARE(zoomFactorSpinBox->minimum(), 0.1);
  QCOMPARE(zoomFactorSpinBox->maximum(), 8.0);

  const QList<QAction*> toolbarActions = toolbar->actions();
  QCOMPARE(toolbarActions.size(), qsizetype{14});
  QVERIFY(toolbarActions.at(1)->isSeparator());
  QCOMPARE(toolbarActions.at(2), undoAction);
  QCOMPARE(toolbarActions.at(3), redoAction);
  QVERIFY(toolbarActions.at(4)->isSeparator());
  QCOMPARE(toolbarActions.at(5), rotateLeftAction);
  QCOMPARE(toolbarActions.at(6), rotateRightAction);
  QCOMPARE(toolbar->widgetForAction(toolbarActions.at(7)), zoomFactorSpinBox);
  QVERIFY(toolbarActions.at(8)->isSeparator());
  QCOMPARE(toolbarActions.at(9), rectangleAction);
  QCOMPARE(toolbarActions.at(10), ellipseAction);
  QCOMPARE(toolbarActions.at(11), circleAction);
  QCOMPARE(toolbarActions.at(12), polygonAction);
  QCOMPARE(toolbarActions.at(13), bezierCurveAction);
}

void MainWindowTest::remembersLastOpenDirectory() {
  constexpr auto settingsKey = "image/lastOpenDirectory";
  bool hadPreviousValue = false;
  QVariant previousValue;
  {
    QSettings settings;
    hadPreviousValue = settings.contains(QString::fromLatin1(settingsKey));
    previousValue = settings.value(QString::fromLatin1(settingsKey));
    settings.remove(QString::fromLatin1(settingsKey));
    settings.sync();
  }

  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());
  const QString imageDirectory = temporaryDirectory.filePath("images");
  QVERIFY(QDir{}.mkpath(imageDirectory));
  QImage sourceImage({20, 20}, QImage::Format_RGB32);
  sourceImage.fill(Qt::red);
  const QString imagePath = QDir{imageDirectory}.filePath("remember.png");
  QVERIFY(sourceImage.save(imagePath));

  quickshot::MainWindow window;
  auto* openButton = window.findChild<QPushButton*>("openButton");
  QVERIFY(openButton != nullptr);
  window.show();
  QCoreApplication::processEvents();

  bool firstDialogFound = false;
  QTimer::singleShot(0, [&firstDialogFound, &imagePath]() {
    auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
    if (dialog == nullptr) {
      return;
    }
    firstDialogFound = true;
    dialog->selectFile(imagePath);
    QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
  });
  QTest::mouseClick(openButton, Qt::LeftButton);
  QCoreApplication::processEvents();

  QSettings persistedSettings;
  persistedSettings.sync();
  const QString savedDirectory =
      persistedSettings.value(QString::fromLatin1(settingsKey)).toString();
  bool secondDialogFound = false;
  QString secondDialogDirectory;
  QTimer::singleShot(0, [&secondDialogFound, &secondDialogDirectory]() {
    auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
    if (dialog == nullptr) {
      return;
    }
    secondDialogFound = true;
    secondDialogDirectory = dialog->directory().absolutePath();
    dialog->reject();
  });
  QTest::mouseClick(openButton, Qt::LeftButton);
  QCoreApplication::processEvents();

  if (hadPreviousValue) {
    persistedSettings.setValue(QString::fromLatin1(settingsKey), previousValue);
  } else {
    persistedSettings.remove(QString::fromLatin1(settingsKey));
  }
  persistedSettings.sync();

  QVERIFY(firstDialogFound);
  QCOMPARE(QFileInfo{savedDirectory}.canonicalFilePath(),
           QFileInfo{imageDirectory}.canonicalFilePath());
  QVERIFY(secondDialogFound);
  QCOMPARE(QFileInfo{secondDialogDirectory}.canonicalFilePath(),
           QFileInfo{imageDirectory}.canonicalFilePath());
}

void MainWindowTest::synchronizesToolbarZoomControl() {
  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());

  QImage sourceImage({200, 150}, QImage::Format_RGB32);
  sourceImage.fill(Qt::black);
  const QString imagePath = temporaryDirectory.filePath("toolbar-zoom.png");
  QVERIFY(sourceImage.save(imagePath));

  quickshot::MainWindow window;
  auto* drawWidget = window.findChild<quickshot::QDrawWidget*>("drawWidget");
  auto* zoomFactorSpinBox = window.findChild<QDoubleSpinBox*>("zoomFactorSpinBox");
  QVERIFY(drawWidget != nullptr);
  QVERIFY(zoomFactorSpinBox != nullptr);
  window.show();
  QCoreApplication::processEvents();
  QVERIFY(drawWidget->loadImage(imagePath));

  QVERIFY(zoomFactorSpinBox->isEnabled());
  zoomFactorSpinBox->clearFocus();
  sendWheel(*zoomFactorSpinBox, 120);
  QCOMPARE(zoomFactorSpinBox->value(), 1.1);
  QCOMPARE(drawWidget->zoomFactor(), 1.1);

  zoomFactorSpinBox->setValue(2.5);
  QCOMPARE(drawWidget->zoomFactor(), 2.5);

  sendControlWheel(*drawWidget, 120);
  QCOMPARE(zoomFactorSpinBox->value(), drawWidget->zoomFactor());

  QVERIFY(drawWidget->loadImage(imagePath));
  QCOMPARE(drawWidget->zoomFactor(), 1.0);
  QCOMPARE(zoomFactorSpinBox->value(), 1.0);
}

void MainWindowTest::showsImageCoordinatesInStatusBar() {
  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());

  QImage sourceImage({400, 300}, QImage::Format_RGB32);
  sourceImage.fill(Qt::black);
  const QString imagePath = temporaryDirectory.filePath("coordinates.png");
  QVERIFY(sourceImage.save(imagePath));

  quickshot::MainWindow window;
  window.resize(220, 200);
  auto* drawWidget = window.findChild<quickshot::QDrawWidget*>("drawWidget");
  auto* coordinateLabel = window.findChild<QLabel*>("coordinateLabel");
  QVERIFY(drawWidget != nullptr);
  QVERIFY(coordinateLabel != nullptr);
  QCOMPARE(coordinateLabel->text(), QStringLiteral("X: —  Y: —"));

  window.show();
  QCoreApplication::processEvents();
  QVERIFY(drawWidget->loadImage(imagePath));
  drawWidget->setZoomFactor(2.0);
  drawWidget->horizontalScrollBar()->setValue(80);
  drawWidget->verticalScrollBar()->setValue(40);

  QTest::mouseMove(drawWidget->viewport(), {20, 20});
  QCoreApplication::processEvents();
  QCOMPARE(coordinateLabel->text(), QStringLiteral("X: 50  Y: 30"));

  drawWidget->setZoomFactor(1.0);
  window.resize(720, 420);
  QCoreApplication::processEvents();
  QTest::mouseMove(drawWidget->viewport(), {600, 200});
  QCoreApplication::processEvents();
  QCOMPARE(coordinateLabel->text(), QStringLiteral("X: —  Y: —"));
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
  auto* circleAction = window.findChild<QAction*>("circleAction");
  auto* polygonAction = window.findChild<QAction*>("polygonAction");
  auto* bezierCurveAction = window.findChild<QAction*>("bezierCurveAction");
  QVERIFY(drawWidget != nullptr);
  QVERIFY(rotateLeftAction != nullptr);
  QVERIFY(rotateRightAction != nullptr);
  QVERIFY(rectangleAction != nullptr);
  QVERIFY(ellipseAction != nullptr);
  QVERIFY(circleAction != nullptr);
  QVERIFY(polygonAction != nullptr);
  QVERIFY(bezierCurveAction != nullptr);

  window.resize(180, 180);
  window.show();
  QCoreApplication::processEvents();
  QVERIFY(drawWidget->loadImage(imagePath));

  QVERIFY(rotateLeftAction->isEnabled());
  QVERIFY(rotateRightAction->isEnabled());
  QVERIFY(rectangleAction->isEnabled());
  QVERIFY(ellipseAction->isEnabled());
  QVERIFY(circleAction->isEnabled());
  QVERIFY(polygonAction->isEnabled());
  QVERIFY(bezierCurveAction->isEnabled());
  QVERIFY(drawWidget->horizontalScrollBar()->maximum() > 0);
  QCOMPARE(drawWidget->verticalScrollBar()->maximum(), 0);

  rotateRightAction->trigger();
  QCOMPARE(drawWidget->horizontalScrollBar()->maximum(), 0);
  QVERIFY(drawWidget->verticalScrollBar()->maximum() > 0);

  rotateLeftAction->trigger();
  QVERIFY(drawWidget->horizontalScrollBar()->maximum() > 0);
  QCOMPARE(drawWidget->verticalScrollBar()->maximum(), 0);
}

void MainWindowTest::undoesAndRedoesDragOperations() {
  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());

  QImage sourceImage({200, 150}, QImage::Format_RGB32);
  sourceImage.fill(Qt::black);
  const QString imagePath = temporaryDirectory.filePath("drag-undo.png");
  QVERIFY(sourceImage.save(imagePath));

  quickshot::MainWindow window;
  auto* drawWidget = window.findChild<quickshot::QDrawWidget*>("drawWidget");
  auto* undoAction = window.findChild<QAction*>("undoAction");
  auto* redoAction = window.findChild<QAction*>("redoAction");
  QVERIFY(drawWidget != nullptr);
  QVERIFY(undoAction != nullptr);
  QVERIFY(redoAction != nullptr);
  window.resize(220, 180);
  QVERIFY(drawWidget->loadImage(imagePath));
  window.show();
  QCoreApplication::processEvents();

  drawWidget->setCreationMode(quickshot::ShapeType::Rectangle, true);
  drag(drawWidget->viewport(), {20, 20}, {80, 60});
  QCOMPARE(drawWidget->undoStack().count(), 1);
  QCOMPARE(drawWidget->undoStack().undoText(), QStringLiteral("Create Shape"));
  QCOMPARE(drawWidget->shapeCount(), qsizetype{1});
  QVERIFY(undoAction->isEnabled());
  QVERIFY(!redoAction->isEnabled());

  undoAction->trigger();
  QCOMPARE(drawWidget->shapeCount(), qsizetype{0});
  QVERIFY(redoAction->isEnabled());
  redoAction->trigger();
  QCOMPARE(drawWidget->shapeCount(), qsizetype{1});
  QCOMPARE(drawWidget->shapeAt(0)->boundingRect(), QRectF(20.0, 20.0, 60.0, 40.0));

  QTest::mousePress(drawWidget->viewport(), Qt::LeftButton, Qt::NoModifier, {50, 40});
  QTest::mouseMove(drawWidget->viewport(), {60, 50});
  undoAction->trigger();
  QCOMPARE(drawWidget->shapeCount(), qsizetype{0});
  QTest::mouseRelease(drawWidget->viewport(), Qt::LeftButton, Qt::NoModifier, {60, 50});
  redoAction->trigger();
  QCOMPARE(drawWidget->shapeAt(0)->boundingRect(), QRectF(20.0, 20.0, 60.0, 40.0));

  drag(drawWidget->viewport(), {50, 40}, {60, 50});
  QCOMPARE(drawWidget->undoStack().count(), 2);
  QCOMPARE(drawWidget->undoStack().undoText(), QStringLiteral("Move Shape"));
  QCOMPARE(drawWidget->shapeAt(0)->boundingRect(), QRectF(30.0, 30.0, 60.0, 40.0));
  drawWidget->undoStack().undo();
  QCOMPARE(drawWidget->shapeAt(0)->boundingRect(), QRectF(20.0, 20.0, 60.0, 40.0));
  drawWidget->undoStack().redo();
  QCOMPARE(drawWidget->shapeAt(0)->boundingRect(), QRectF(30.0, 30.0, 60.0, 40.0));

  drag(drawWidget->viewport(), {90, 70}, {110, 90});
  QCOMPARE(drawWidget->undoStack().count(), 3);
  QCOMPARE(drawWidget->undoStack().undoText(), QStringLiteral("Resize Shape"));
  QCOMPARE(drawWidget->shapeAt(0)->boundingRect(), QRectF(30.0, 30.0, 80.0, 60.0));
  drawWidget->undoStack().undo();
  QCOMPARE(drawWidget->shapeAt(0)->boundingRect(), QRectF(30.0, 30.0, 60.0, 40.0));
  drawWidget->undoStack().redo();
  QCOMPARE(drawWidget->shapeAt(0)->boundingRect(), QRectF(30.0, 30.0, 80.0, 60.0));

  QTest::mousePress(drawWidget->viewport(), Qt::RightButton, Qt::NoModifier, {110, 60});
  QTest::mouseMove(drawWidget->viewport(), {70, 100});
  QTest::mouseRelease(drawWidget->viewport(), Qt::RightButton, Qt::NoModifier, {70, 100});
  QCoreApplication::processEvents();
  QCOMPARE(drawWidget->undoStack().count(), 4);
  QCOMPARE(drawWidget->undoStack().undoText(), QStringLiteral("Rotate Shape"));
  QVERIFY(qAbs(drawWidget->shapeAt(0)->rotationDegrees() - 90.0) < 0.0001);
  drawWidget->undoStack().undo();
  QCOMPARE(drawWidget->shapeAt(0)->rotationDegrees(), 0.0);
  drawWidget->undoStack().redo();
  QVERIFY(qAbs(drawWidget->shapeAt(0)->rotationDegrees() - 90.0) < 0.0001);
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

  drawWidget.setCreationMode(quickshot::ShapeType::Rectangle, true);
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

  drawWidget.setCreationMode(quickshot::ShapeType::Rectangle, false);
  drawWidget.setCreationMode(quickshot::ShapeType::Ellipse, true);
  drag(drawWidget.viewport(), {120, 20}, {180, 80});
  QCOMPARE(drawWidget.shapeCount(), qsizetype{2});
  QCOMPARE(drawWidget.shapeAt(1)->boundingRect(), QRectF(120.0, 20.0, 60.0, 60.0));
  QCOMPARE(drawWidget.shapeAt(1)->handles().size(), std::size_t{8});

  QTest::mouseMove(drawWidget.viewport(), {150, 20});
  QCOMPARE(drawWidget.viewport()->cursor().shape(), Qt::SizeVerCursor);

  drawWidget.rotateRight();
  QCOMPARE(drawWidget.shapeAt(0)->path().boundingRect().size(), QSizeF(70.0, 80.0));
  QCOMPARE(drawWidget.shapeAt(0)->rotationDegrees(), 90.0);
}

void MainWindowTest::createsCirclesAndPolygons() {
  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());

  QImage sourceImage({200, 150}, QImage::Format_RGB32);
  sourceImage.fill(Qt::black);
  const QString imagePath = temporaryDirectory.filePath("circle-polygon.png");
  QVERIFY(sourceImage.save(imagePath));

  quickshot::QDrawWidget drawWidget;
  drawWidget.resize(220, 180);
  QVERIFY(drawWidget.loadImage(imagePath));
  drawWidget.show();
  QCoreApplication::processEvents();

  drawWidget.setCreationMode(quickshot::ShapeType::Circle, true);
  drag(drawWidget.viewport(), {20, 20}, {70, 50});
  QCOMPARE(drawWidget.shapeCount(), qsizetype{1});
  const auto* circle = dynamic_cast<const quickshot::Circle*>(drawWidget.shapeAt(0));
  QVERIFY(circle != nullptr);
  QCOMPARE(circle->boundingRect(), QRectF(20.0, 20.0, 50.0, 50.0));

  drawWidget.setCreationMode(quickshot::ShapeType::Polygon, true);
  QTest::mouseClick(drawWidget.viewport(), Qt::LeftButton, Qt::NoModifier, {100, 20});
  QTest::mouseClick(drawWidget.viewport(), Qt::LeftButton, Qt::NoModifier, {170, 20});
  QTest::mouseClick(drawWidget.viewport(), Qt::LeftButton, Qt::NoModifier, {130, 90});
  QTest::mouseMove(drawWidget.viewport(), {180, 110});
  QCoreApplication::processEvents();

  const QColor creationHandleColor{0, 200, 83};
  const QImage draft = renderViewport(drawWidget);
  QVERIFY(hasColorNear(draft, {100, 20}, creationHandleColor));
  QVERIFY(hasColorNear(draft, {170, 20}, creationHandleColor));
  QVERIFY(hasColorNear(draft, {130, 90}, creationHandleColor));

  // Right-clicking elsewhere completes the polygon without adding that position as a vertex.
  QTest::mouseClick(drawWidget.viewport(), Qt::RightButton, Qt::NoModifier, {190, 130});
  QCoreApplication::processEvents();

  QCOMPARE(drawWidget.shapeCount(), qsizetype{2});
  const auto* polygon = dynamic_cast<const quickshot::Polygon*>(drawWidget.shapeAt(1));
  QVERIFY(polygon != nullptr);
  QVERIFY(polygon->isCreationComplete());
  QCOMPARE(polygon->pointCount(), qsizetype{3});
  QCOMPARE(polygon->handles().size(), std::size_t{3});
  QCOMPARE(polygon->handleCenter(polygon->handles().back()), QPointF(130.0, 90.0));

  const QImage completed = renderViewport(drawWidget);
  QVERIFY(hasColorNear(completed, {100, 20}, Qt::white));
  QVERIFY(!hasColorNear(completed, {100, 20}, creationHandleColor));
  QCOMPARE(drawWidget.undoStack().count(), 2);
  drawWidget.undoStack().undo();
  QCOMPARE(drawWidget.shapeCount(), qsizetype{1});
  drawWidget.undoStack().redo();
  QCOMPARE(drawWidget.shapeCount(), qsizetype{2});
}

void MainWindowTest::createsBezierCurves() {
  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());

  QImage sourceImage({200, 150}, QImage::Format_RGB32);
  sourceImage.fill(Qt::black);
  const QString imagePath = temporaryDirectory.filePath("bezier-curve.png");
  QVERIFY(sourceImage.save(imagePath));

  quickshot::QDrawWidget drawWidget;
  drawWidget.resize(220, 180);
  QVERIFY(drawWidget.loadImage(imagePath));
  drawWidget.show();
  QCoreApplication::processEvents();

  drawWidget.setCreationMode(quickshot::ShapeType::BezierCurve, true);
  QTest::mouseClick(drawWidget.viewport(), Qt::LeftButton, Qt::NoModifier, {30, 30});
  QTest::mouseClick(drawWidget.viewport(), Qt::LeftButton, Qt::NoModifier, {160, 30});
  QTest::mouseClick(drawWidget.viewport(), Qt::LeftButton, Qt::NoModifier, {160, 110});
  QTest::mouseClick(drawWidget.viewport(), Qt::LeftButton, Qt::NoModifier, {30, 110});
  QTest::mouseMove(drawWidget.viewport(), {100, 130});
  QCoreApplication::processEvents();

  const QColor creationHandleColor{0, 200, 83};
  const QImage draft = renderViewport(drawWidget);
  QVERIFY(hasColorNear(draft, {30, 30}, creationHandleColor));
  QVERIFY(hasColorNear(draft, {160, 110}, creationHandleColor));

  // Right-click completes the curve; its position is not appended as an anchor.
  QTest::mouseClick(drawWidget.viewport(), Qt::RightButton, Qt::NoModifier, {190, 140});
  QCoreApplication::processEvents();

  QCOMPARE(drawWidget.shapeCount(), qsizetype{1});
  const auto* curve = dynamic_cast<const quickshot::BezierCurve*>(drawWidget.shapeAt(0));
  QVERIFY(curve != nullptr);
  QVERIFY(curve->isCreationComplete());
  QCOMPARE(curve->pointCount(), qsizetype{4});
  QCOMPARE(curve->handles().size(), std::size_t{4});
  QVERIFY(curve->path().elementAt(1).isCurveTo());

  const QImage completed = renderViewport(drawWidget);
  QVERIFY(hasColorNear(completed, {30, 30}, Qt::white));
  QVERIFY(!hasColorNear(completed, {30, 30}, creationHandleColor));
  QCOMPARE(drawWidget.undoStack().count(), 1);
  drawWidget.undoStack().undo();
  QCOMPARE(drawWidget.shapeCount(), qsizetype{0});
  drawWidget.undoStack().redo();
  QCOMPARE(drawWidget.shapeCount(), qsizetype{1});
}

void MainWindowTest::switchesIndependentImageDocuments() {
  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());

  QImage firstImage({160, 120}, QImage::Format_RGB32);
  firstImage.fill(Qt::black);
  QImage secondImage({180, 140}, QImage::Format_RGB32);
  secondImage.fill(Qt::blue);
  QImage thirdImage({200, 160}, QImage::Format_RGB32);
  thirdImage.fill(Qt::green);
  const QString firstPath = temporaryDirectory.filePath("first.png");
  const QString secondPath = temporaryDirectory.filePath("second.png");
  const QString thirdPath = temporaryDirectory.filePath("third.png");
  QVERIFY(firstImage.save(firstPath));
  QVERIFY(secondImage.save(secondPath));
  QVERIFY(thirdImage.save(thirdPath));

  quickshot::MainWindow window;
  auto* drawWidget = window.findChild<quickshot::QDrawWidget*>("drawWidget");
  auto* imageDock = window.findChild<QDockWidget*>("imageDockWidget");
  auto* imageList = window.findChild<QListWidget*>("imageListView");
  QVERIFY(drawWidget != nullptr);
  QVERIFY(imageDock != nullptr);
  QVERIFY(imageList != nullptr);
  window.show();
  QCoreApplication::processEvents();

  QVERIFY(drawWidget->loadImage(firstPath));
  QCoreApplication::processEvents();
  QCOMPARE(drawWidget->imageCount(), qsizetype{1});
  QCOMPARE(drawWidget->currentImageIndex(), qsizetype{0});
  QCOMPARE(imageList->count(), 1);
  QVERIFY(!imageList->item(0)->icon().isNull());
  QVERIFY(imageDock->isHidden());
  QCOMPARE(imageList->flow(), QListView::TopToBottom);
  QVERIFY(imageList->styleSheet().contains(QStringLiteral("border")));

  drawWidget->setCreationMode(quickshot::ShapeType::Rectangle, true);
  drag(drawWidget->viewport(), {20, 20}, {80, 70});
  QCOMPARE(drawWidget->shapeCount(), qsizetype{1});
  QCOMPARE(drawWidget->undoStack().count(), 1);

  QVERIFY(drawWidget->loadImages({secondPath, thirdPath}).isEmpty());
  QCoreApplication::processEvents();
  QCOMPARE(drawWidget->imageCount(), qsizetype{3});
  QCOMPARE(drawWidget->currentImageIndex(), qsizetype{1});
  QCOMPARE(imageList->count(), 3);
  QVERIFY(!imageList->item(1)->icon().isNull());
  QVERIFY(!imageList->item(2)->icon().isNull());
  QVERIFY(!imageDock->isHidden());
  QCOMPARE(drawWidget->shapeCount(), qsizetype{0});
  QCOMPARE(drawWidget->undoStack().count(), 0);
  drag(drawWidget->viewport(), {90, 30}, {150, 100});
  QCOMPARE(drawWidget->shapeCount(), qsizetype{1});
  QCOMPARE(drawWidget->undoStack().count(), 1);

  imageList->setCurrentRow(0);
  QCoreApplication::processEvents();
  QCOMPARE(drawWidget->shapeCount(), qsizetype{1});
  QCOMPARE(drawWidget->shapeAt(0)->boundingRect(), QRectF(20.0, 20.0, 60.0, 50.0));
  QCOMPARE(drawWidget->undoStack().count(), 1);

  imageList->setCurrentRow(1);
  QCoreApplication::processEvents();
  QCOMPARE(drawWidget->shapeCount(), qsizetype{1});
  QCOMPARE(drawWidget->undoStack().count(), 1);

  QVERIFY(triggerImageListContextMenuAction(*imageList, 0, "deleteImageAction"));
  QCOMPARE(drawWidget->imageCount(), qsizetype{2});
  QCOMPARE(drawWidget->currentImageIndex(), qsizetype{0});
  QCOMPARE(drawWidget->imagePathAt(0), secondPath);
  QCOMPARE(drawWidget->shapeCount(), qsizetype{1});
  QCOMPARE(drawWidget->undoStack().count(), 1);

  QVERIFY(triggerImageListContextMenuAction(*imageList, 0, "deleteImageAction"));
  QCOMPARE(drawWidget->imageCount(), qsizetype{1});
  QCOMPARE(drawWidget->currentImageIndex(), qsizetype{0});
  QCOMPARE(drawWidget->imagePathAt(0), thirdPath);
  QCOMPARE(drawWidget->shapeCount(), qsizetype{0});
  QCOMPARE(imageList->count(), 1);
  QVERIFY(imageDock->isHidden());

  drawWidget->removeImage(0);
  QCOMPARE(drawWidget->imageCount(), qsizetype{0});
  QCOMPARE(drawWidget->currentImageIndex(), qsizetype{-1});
  QVERIFY(!drawWidget->hasImage());
  QCOMPARE(imageList->count(), 0);
}

void MainWindowTest::batchSaveUsesOnlyCurrentImageShapes() {
  constexpr auto settingsKey = "roi/lastSaveDirectory";
  QSettings settings;
  const bool hadPreviousValue = settings.contains(QString::fromLatin1(settingsKey));
  const QVariant previousValue = settings.value(QString::fromLatin1(settingsKey));

  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());
  const QString outputDirectory = temporaryDirectory.filePath("output");
  QVERIFY(QDir{}.mkpath(outputDirectory));

  QImage currentImage({100, 100}, QImage::Format_RGB32);
  currentImage.fill(Qt::red);
  QImage largeImage({200, 200}, QImage::Format_RGB32);
  largeImage.fill(Qt::green);
  QImage smallImage({50, 50}, QImage::Format_RGB32);
  smallImage.fill(Qt::blue);
  const QString currentPath = temporaryDirectory.filePath("current.png");
  const QString largePath = temporaryDirectory.filePath("large.png");
  const QString smallPath = temporaryDirectory.filePath("small.png");
  QVERIFY(currentImage.save(currentPath));
  QVERIFY(largeImage.save(largePath));
  QVERIFY(smallImage.save(smallPath));

  quickshot::QDrawWidget drawWidget;
  drawWidget.resize(220, 180);
  QVERIFY(drawWidget.loadImages({currentPath, largePath, smallPath}).isEmpty());
  drawWidget.show();
  QCoreApplication::processEvents();
  drawWidget.setCreationMode(quickshot::ShapeType::Rectangle, true);
  drag(drawWidget.viewport(), {10, 10}, {80, 80});

  drawWidget.setCurrentImageIndex(1);
  drag(drawWidget.viewport(), {150, 150}, {190, 190});
  QCOMPARE(drawWidget.shapeCount(), qsizetype{1});
  drawWidget.setCurrentImageIndex(0);
  QCOMPARE(drawWidget.shapeCount(), qsizetype{1});

  const BatchDialogSnapshot selectedSnapshot =
      inspectBatchContextAction(drawWidget, {20, 20}, "batchSaveRoiAction");
  QVERIFY(selectedSnapshot.actionFound);
  QVERIFY(selectedSnapshot.dialogFound);
  QCOMPARE(selectedSnapshot.statuses,
           QStringList({QStringLiteral("✓"), QStringLiteral("✓"), QStringLiteral("✗")}));
  QVERIFY(selectedSnapshot.imageCells.at(0).contains(currentPath));
  QVERIFY(selectedSnapshot.imageCells.at(1).contains(largePath));
  QVERIFY(selectedSnapshot.imageCells.at(2).contains(smallPath));

  drag(drawWidget.viewport(), {5, 85}, {30, 98});
  QCOMPARE(drawWidget.shapeCount(), qsizetype{2});
  const BatchDialogSnapshot allSnapshot =
      inspectBatchContextAction(drawWidget, {95, 5}, "batchSaveAllRoisAction", outputDirectory);
  QVERIFY(allSnapshot.actionFound);
  QVERIFY(allSnapshot.dialogFound);
  QCOMPARE(allSnapshot.statuses,
           QStringList({QStringLiteral("✓"), QStringLiteral("✓"), QStringLiteral("✗")}));

  const QStringList outputFiles =
      QDir{outputDirectory}.entryList({QStringLiteral("*.png")}, QDir::Files, QDir::Name);
  QCOMPARE(outputFiles.size(), 4);
  const QRegularExpression outputPattern{
      QStringLiteral("^(current|large)_roi_\\d{8}_\\d{6}_\\d{3}_00[1-4]\\.png$")};
  for (const QString& outputFile : outputFiles) {
    QVERIFY2(outputPattern.match(outputFile).hasMatch(), qPrintable(outputFile));
  }

  if (hadPreviousValue) {
    settings.setValue(QString::fromLatin1(settingsKey), previousValue);
  } else {
    settings.remove(QString::fromLatin1(settingsKey));
  }
  settings.sync();
}

void MainWindowTest::hidesInactiveHandlesWhileResizing() {
  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());

  QImage sourceImage({200, 150}, QImage::Format_RGB32);
  sourceImage.fill(Qt::black);
  const QString imagePath = temporaryDirectory.filePath("handles.png");
  QVERIFY(sourceImage.save(imagePath));

  quickshot::QDrawWidget drawWidget;
  drawWidget.resize(220, 180);
  QVERIFY(drawWidget.loadImage(imagePath));
  drawWidget.show();
  QCoreApplication::processEvents();
  drawWidget.setCreationMode(quickshot::ShapeType::Ellipse, true);
  drag(drawWidget.viewport(), {40, 30}, {140, 110});

  const QRect imageArea{0, 0, sourceImage.width(), sourceImage.height()};
  const qsizetype restingWhitePixels =
      colorPixelCount(renderViewport(drawWidget), Qt::white, imageArea);
  QVERIFY(restingWhitePixels > 0);

  QTest::mousePress(drawWidget.viewport(), Qt::LeftButton, Qt::NoModifier, {140, 110});
  QCoreApplication::processEvents();
  const qsizetype draggingWhitePixels =
      colorPixelCount(renderViewport(drawWidget), Qt::white, imageArea);
  QVERIFY(draggingWhitePixels * 2 < restingWhitePixels);

  QTest::mouseRelease(drawWidget.viewport(), Qt::LeftButton, Qt::NoModifier, {140, 110});
  QCoreApplication::processEvents();
  const qsizetype releasedWhitePixels =
      colorPixelCount(renderViewport(drawWidget), Qt::white, imageArea);
  QCOMPARE(releasedWhitePixels, restingWhitePixels);
}

void MainWindowTest::contextMenusCloneAndDeleteShapes() {
  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());

  QImage sourceImage({200, 150}, QImage::Format_RGB32);
  sourceImage.fill(Qt::black);
  const QString imagePath = temporaryDirectory.filePath("context-menu.png");
  QVERIFY(sourceImage.save(imagePath));

  quickshot::QDrawWidget drawWidget;
  drawWidget.resize(220, 180);
  QVERIFY(drawWidget.loadImage(imagePath));
  drawWidget.show();
  QCoreApplication::processEvents();
  drawWidget.setCreationMode(quickshot::ShapeType::Rectangle, true);
  drag(drawWidget.viewport(), {40, 30}, {100, 90});

  QVERIFY(triggerContextMenuAction(drawWidget, {60, 50}, "cloneShapeAction"));
  QCOMPARE(drawWidget.shapeCount(), qsizetype{2});
  QCOMPARE(drawWidget.undoStack().count(), 2);
  QCOMPARE(drawWidget.shapeAt(1)->boundingRect(), QRectF(30.0, 30.0, 60.0, 60.0));
  const QImage clonedShapes = renderViewport(drawWidget);
  QVERIFY(hasColorNear(clonedShapes, {30, 45}, Qt::red));
  QVERIFY(hasColorNear(clonedShapes, {100, 45}, Qt::white));

  drawWidget.undoStack().undo();
  QCOMPARE(drawWidget.shapeCount(), qsizetype{1});
  drawWidget.undoStack().redo();
  QCOMPARE(drawWidget.shapeCount(), qsizetype{2});
  QCOMPARE(drawWidget.shapeAt(1)->boundingRect(), QRectF(30.0, 30.0, 60.0, 60.0));

  QVERIFY(triggerContextMenuAction(drawWidget, {35, 50}, "deleteShapeAction"));
  QCOMPARE(drawWidget.shapeCount(), qsizetype{1});
  QCOMPARE(drawWidget.undoStack().count(), 3);

  drawWidget.undoStack().undo();
  QCOMPARE(drawWidget.shapeCount(), qsizetype{2});
  QCOMPARE(drawWidget.shapeAt(1)->boundingRect(), QRectF(30.0, 30.0, 60.0, 60.0));
  drawWidget.undoStack().redo();
  QCOMPARE(drawWidget.shapeCount(), qsizetype{1});

  QVERIFY(triggerContextMenuAction(drawWidget, {180, 130}, "deleteAllShapesAction"));
  QCOMPARE(drawWidget.shapeCount(), qsizetype{0});
  QCOMPARE(drawWidget.undoStack().count(), 4);

  drawWidget.undoStack().undo();
  QCOMPARE(drawWidget.shapeCount(), qsizetype{1});
  drawWidget.undoStack().redo();
  QCOMPARE(drawWidget.shapeCount(), qsizetype{0});

  QVERIFY(drawWidget.loadImage(imagePath));
  QCOMPARE(drawWidget.undoStack().count(), 0);
}

void MainWindowTest::resizesAndRotatesFromShapeHandles() {
  QTemporaryDir temporaryDirectory;
  QVERIFY(temporaryDirectory.isValid());

  QImage sourceImage({200, 150}, QImage::Format_RGB32);
  sourceImage.fill(Qt::black);
  const QString imagePath = temporaryDirectory.filePath("shape-rotation.png");
  QVERIFY(sourceImage.save(imagePath));

  quickshot::QDrawWidget drawWidget;
  drawWidget.resize(220, 180);
  QVERIFY(drawWidget.loadImage(imagePath));
  drawWidget.show();
  QCoreApplication::processEvents();
  drawWidget.setCreationMode(quickshot::ShapeType::Rectangle, true);
  drag(drawWidget.viewport(), {40, 30}, {100, 90});

  QTest::mouseMove(drawWidget.viewport(), {100, 60});
  QCoreApplication::processEvents();
  QCOMPARE(drawWidget.viewport()->cursor().shape(), Qt::SizeHorCursor);

  const QRect imageArea{0, 0, sourceImage.width(), sourceImage.height()};
  const qsizetype restingWhitePixels =
      colorPixelCount(renderViewport(drawWidget), Qt::white, imageArea);
  QTest::mousePress(drawWidget.viewport(), Qt::RightButton, Qt::NoModifier, {100, 60});
  QCoreApplication::processEvents();
  QCOMPARE(drawWidget.viewport()->cursor().shape(), Qt::BitmapCursor);
  const qsizetype rotatingWhitePixels =
      colorPixelCount(renderViewport(drawWidget), Qt::white, imageArea);
  QVERIFY(rotatingWhitePixels < restingWhitePixels);
  QTest::mouseMove(drawWidget.viewport(), {70, 90});
  QTest::mouseRelease(drawWidget.viewport(), Qt::RightButton, Qt::NoModifier, {70, 90});
  QCoreApplication::processEvents();

  QVERIFY(qAbs(drawWidget.shapeAt(0)->rotationDegrees() - 90.0) < 0.0001);
  const QPointF fixedCorner = drawWidget.shapeAt(0)->handleCenter(
      quickshot::ShapeHandle{quickshot::HandlePosition::BottomRight});
  drag(drawWidget.viewport(), {100, 30}, {110, 20});
  const QPointF resizedFixedCorner = drawWidget.shapeAt(0)->handleCenter(
      quickshot::ShapeHandle{quickshot::HandlePosition::BottomRight});
  const qreal fixedCornerMovement = QLineF{fixedCorner, resizedFixedCorner}.length();
  QVERIFY(fixedCornerMovement < 0.0001);
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

  drawWidget.setCreationMode(quickshot::ShapeType::Rectangle, true);
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
  QCOMPARE(renderedImage.pixelColor(40, 19), QColor(Qt::white));
  QCOMPARE(renderedImage.pixelColor(39, 20), QColor(Qt::white));
  QCOMPARE(renderedImage.pixelColor(99, 99), QColor(Qt::white));
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
