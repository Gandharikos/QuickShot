#include "quickshot/main_window.hpp"

#include "quickshot/canvas_view.hpp"
#include "quickshot/shapes/shape.hpp"

#include <QAction>
#include <QActionGroup>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QImageReader>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStringList>
#include <QStyle>
#include <QSvgRenderer>
#include <QToolBar>
#include <QtMath>

namespace quickshot {

namespace {

constexpr auto lastOpenDirectoryKey = "image/lastOpenDirectory";
constexpr QSize thumbnailSize{120, 90};

QString defaultOpenDirectory() {
  QString directory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
  if (directory.isEmpty()) {
    directory = QDir::homePath();
  }
  return directory;
}

QString lastOpenDirectory() {
  const QString savedDirectory =
      QSettings{}.value(QString::fromLatin1(lastOpenDirectoryKey)).toString();
  return !savedDirectory.isEmpty() && QDir{savedDirectory}.exists() ? savedDirectory
                                                                    : defaultOpenDirectory();
}

QString imageFilter() {
  QStringList patterns;
  for (const QByteArray& format : QImageReader::supportedImageFormats()) {
    patterns.append("*." + QString::fromLatin1(format));
  }
  patterns.removeDuplicates();
  patterns.sort();
  return MainWindow::tr("Images (%1)").arg(patterns.join(' '));
}

QIcon svgIcon(const QString& resource) {
  QSvgRenderer renderer{resource};
  QPixmap fallbackPixmap{24, 24};
  fallbackPixmap.fill(Qt::transparent);
  {
    QPainter painter{&fallbackPixmap};
    renderer.render(&painter);
  }
  return QIcon{fallbackPixmap};
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), canvasView_(new CanvasView(this)) {
  setWindowTitle(tr("Quickshot"));
  resize(720, 420);

  auto* toolbar = addToolBar(tr("File"));
  auto* openButton = new QPushButton(tr("Open"), toolbar);

  toolbar->setObjectName("mainToolBar");
  toolbar->setMovable(false);
  toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
  openButton->setObjectName("openButton");
  canvasView_->setObjectName("canvasView");

  toolbar->addWidget(openButton);
  toolbar->addSeparator();

  QAction* undoAction = canvasView_->undoGroup().createUndoAction(this, tr("Undo"));
  QAction* redoAction = canvasView_->undoGroup().createRedoAction(this, tr("Redo"));
  undoAction->setObjectName("undoAction");
  redoAction->setObjectName("redoAction");
  undoAction->setIcon(
      QIcon::fromTheme(QStringLiteral("edit-undo"), style()->standardIcon(QStyle::SP_ArrowBack)));
  redoAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-redo"),
                                       style()->standardIcon(QStyle::SP_ArrowForward)));
  undoAction->setShortcut(QKeySequence::Undo);
  redoAction->setShortcut(QKeySequence::Redo);
  toolbar->addAction(undoAction);
  toolbar->addAction(redoAction);
  toolbar->addSeparator();

  auto* rotateLeftAction = toolbar->addAction(
      QIcon::fromTheme(QStringLiteral("object-rotate-left"),
                       svgIcon(QStringLiteral(":/quickshot/icons/rotate-left.svg"))),
      tr("Rotate Left"));
  auto* rotateRightAction = toolbar->addAction(
      QIcon::fromTheme(QStringLiteral("object-rotate-right"),
                       svgIcon(QStringLiteral(":/quickshot/icons/rotate-right.svg"))),
      tr("Rotate Right"));

  rotateLeftAction->setObjectName("rotateLeftAction");
  rotateRightAction->setObjectName("rotateRightAction");
  rotateLeftAction->setEnabled(false);
  rotateRightAction->setEnabled(false);

  auto* zoomFactorSpinBox = new QDoubleSpinBox(toolbar);
  zoomFactorSpinBox->setObjectName("zoomFactorSpinBox");
  zoomFactorSpinBox->setRange(0.1, 8.0);
  zoomFactorSpinBox->setSingleStep(0.1);
  zoomFactorSpinBox->setDecimals(2);
  zoomFactorSpinBox->setValue(1.0);
  zoomFactorSpinBox->setSuffix(QStringLiteral("×"));
  zoomFactorSpinBox->setEnabled(false);
  toolbar->addWidget(zoomFactorSpinBox);

  toolbar->addSeparator();
  auto* shapeActions = new QActionGroup(toolbar);
  shapeActions->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
  auto* rectangleAction = toolbar->addAction(
      QIcon::fromTheme(QStringLiteral("draw-rectangle"),
                       svgIcon(QStringLiteral(":/quickshot/icons/rectangle.svg"))),
      tr("Rectangle"));
  auto* ellipseAction =
      toolbar->addAction(QIcon::fromTheme(QStringLiteral("draw-ellipse"),
                                          svgIcon(QStringLiteral(":/quickshot/icons/ellipse.svg"))),
                         tr("Ellipse"));
  auto* circleAction =
      toolbar->addAction(QIcon::fromTheme(QStringLiteral("draw-circle"),
                                          svgIcon(QStringLiteral(":/quickshot/icons/circle.svg"))),
                         tr("Circle"));
  auto* polygonAction =
      toolbar->addAction(QIcon::fromTheme(QStringLiteral("draw-polygon"),
                                          svgIcon(QStringLiteral(":/quickshot/icons/polygon.svg"))),
                         tr("Polygon"));
  auto* bezierCurveAction = toolbar->addAction(
      QIcon::fromTheme(QStringLiteral("draw-bezier-curves"),
                       svgIcon(QStringLiteral(":/quickshot/icons/bezier-curve.svg"))),
      tr("Bezier Curve"));
  rectangleAction->setObjectName("rectangleAction");
  ellipseAction->setObjectName("ellipseAction");
  circleAction->setObjectName("circleAction");
  polygonAction->setObjectName("polygonAction");
  bezierCurveAction->setObjectName("bezierCurveAction");
  rectangleAction->setCheckable(true);
  ellipseAction->setCheckable(true);
  circleAction->setCheckable(true);
  polygonAction->setCheckable(true);
  bezierCurveAction->setCheckable(true);
  rectangleAction->setEnabled(false);
  ellipseAction->setEnabled(false);
  circleAction->setEnabled(false);
  polygonAction->setEnabled(false);
  bezierCurveAction->setEnabled(false);
  shapeActions->addAction(rectangleAction);
  shapeActions->addAction(ellipseAction);
  shapeActions->addAction(circleAction);
  shapeActions->addAction(polygonAction);
  shapeActions->addAction(bezierCurveAction);

  connect(openButton, &QPushButton::clicked, this, &MainWindow::openImage);
  connect(rotateLeftAction, &QAction::triggered, canvasView_, &CanvasView::rotateLeft);
  connect(rotateRightAction, &QAction::triggered, canvasView_, &CanvasView::rotateRight);
  connect(zoomFactorSpinBox, &QDoubleSpinBox::valueChanged, canvasView_,
          &CanvasView::setZoomFactor);
  connect(canvasView_, &CanvasView::zoomFactorChanged, zoomFactorSpinBox,
          &QDoubleSpinBox::setValue);
  connect(rectangleAction, &QAction::triggered, this,
          [this](bool enabled) { canvasView_->setCreationMode(ShapeType::Rectangle, enabled); });
  connect(ellipseAction, &QAction::triggered, this,
          [this](bool enabled) { canvasView_->setCreationMode(ShapeType::Ellipse, enabled); });
  connect(circleAction, &QAction::triggered, this,
          [this](bool enabled) { canvasView_->setCreationMode(ShapeType::Circle, enabled); });
  connect(polygonAction, &QAction::triggered, this,
          [this](bool enabled) { canvasView_->setCreationMode(ShapeType::Polygon, enabled); });
  connect(bezierCurveAction, &QAction::triggered, this,
          [this](bool enabled) { canvasView_->setCreationMode(ShapeType::BezierCurve, enabled); });
  connect(canvasView_, &CanvasView::imageAvailabilityChanged, rotateLeftAction,
          &QAction::setEnabled);
  connect(canvasView_, &CanvasView::imageAvailabilityChanged, rotateRightAction,
          &QAction::setEnabled);
  connect(canvasView_, &CanvasView::imageAvailabilityChanged, zoomFactorSpinBox,
          &QDoubleSpinBox::setEnabled);
  connect(canvasView_, &CanvasView::imageAvailabilityChanged, rectangleAction,
          &QAction::setEnabled);
  connect(canvasView_, &CanvasView::imageAvailabilityChanged, ellipseAction, &QAction::setEnabled);
  connect(canvasView_, &CanvasView::imageAvailabilityChanged, circleAction, &QAction::setEnabled);
  connect(canvasView_, &CanvasView::imageAvailabilityChanged, polygonAction, &QAction::setEnabled);
  connect(canvasView_, &CanvasView::imageAvailabilityChanged, bezierCurveAction,
          &QAction::setEnabled);

  auto* coordinateLabel = new QLabel(tr("X: —  Y: —"), this);
  coordinateLabel->setObjectName("coordinateLabel");
  statusBar()->addPermanentWidget(coordinateLabel);
  connect(canvasView_, &CanvasView::cursorImagePositionChanged, coordinateLabel,
          [coordinateLabel](const QPointF& position) {
            coordinateLabel->setText(
                MainWindow::tr("X: %1  Y: %2").arg(qFloor(position.x())).arg(qFloor(position.y())));
          });
  connect(canvasView_, &CanvasView::cursorLeftImage, coordinateLabel,
          [coordinateLabel]() { coordinateLabel->setText(MainWindow::tr("X: —  Y: —")); });

  auto* imageDock = new QDockWidget(tr("Images"), this);
  auto* imageList = new QListWidget(imageDock);
  imageDock->setObjectName("imageDockWidget");
  imageDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
  imageDock->setAllowedAreas(Qt::LeftDockWidgetArea);
  imageList->setObjectName("imageListView");
  imageList->setViewMode(QListView::IconMode);
  imageList->setFlow(QListView::TopToBottom);
  imageList->setMovement(QListView::Static);
  imageList->setResizeMode(QListView::Adjust);
  imageList->setWrapping(false);
  imageList->setContextMenuPolicy(Qt::CustomContextMenu);
  imageList->setIconSize(thumbnailSize);
  imageList->setSpacing(6);
  imageList->setFixedWidth(150);
  imageList->setStyleSheet(
      QStringLiteral("QListWidget::item:selected { border: 2px solid #ef5350; }"));
  imageDock->setWidget(imageList);
  addDockWidget(Qt::LeftDockWidgetArea, imageDock);
  imageDock->hide();

  auto* deleteImageAction = new QAction{tr("Delete"), imageList};
  deleteImageAction->setObjectName("deleteImageAction");
  connect(deleteImageAction, &QAction::triggered, this, [this, deleteImageAction]() {
    bool validIndex = false;
    const qsizetype index = deleteImageAction->data().toLongLong(&validIndex);
    if (validIndex) {
      canvasView_->removeImage(index);
    }
  });
  connect(imageList, &QListWidget::customContextMenuRequested, imageList,
          [imageList, deleteImageAction](const QPoint& position) {
            QListWidgetItem* item = imageList->itemAt(position);
            if (item == nullptr) {
              return;
            }

            deleteImageAction->setData(imageList->row(item));
            QMenu menu{imageList};
            menu.addAction(deleteImageAction);
            menu.exec(imageList->viewport()->mapToGlobal(position));
          });

  connect(imageList, &QListWidget::currentRowChanged, canvasView_,
          &CanvasView::setCurrentImageIndex);
  connect(canvasView_, &CanvasView::currentImageChanged, imageList, [imageList](qsizetype index) {
    const QSignalBlocker blocker{imageList};
    imageList->setCurrentRow(static_cast<int>(index));
  });
  connect(canvasView_, &CanvasView::imageCollectionChanged, imageList,
          [this, imageDock, imageList]() {
            const QSignalBlocker blocker{imageList};
            imageList->clear();
            for (qsizetype index = 0; index < canvasView_->imageCount(); ++index) {
              const QString filePath = canvasView_->imagePathAt(index);
              auto* item = new QListWidgetItem{
                  QIcon{QPixmap::fromImage(canvasView_->thumbnailAt(index, thumbnailSize))},
                  QFileInfo{filePath}.fileName(), imageList};
              item->setToolTip(filePath);
              item->setSizeHint({140, 120});
              item->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);
            }
            imageList->setCurrentRow(static_cast<int>(canvasView_->currentImageIndex()));
            imageDock->setVisible(canvasView_->imageCount() > 1);
          });
  connect(canvasView_, &CanvasView::imageThumbnailChanged, imageList,
          [this, imageList](qsizetype index) {
            QListWidgetItem* item = imageList->item(static_cast<int>(index));
            if (item != nullptr) {
              item->setIcon(
                  QIcon{QPixmap::fromImage(canvasView_->thumbnailAt(index, thumbnailSize))});
            }
          });

  setCentralWidget(canvasView_);
}

void MainWindow::openImage() {
  const QStringList fileNames =
      QFileDialog::getOpenFileNames(this, tr("Open Images"), lastOpenDirectory(), imageFilter());
  if (fileNames.isEmpty()) {
    return;
  }

  const QStringList rejectedFiles = canvasView_->loadImages(fileNames);
  if (rejectedFiles.size() == fileNames.size()) {
    QMessageBox::warning(this, tr("Invalid Image"),
                         tr("None of the selected files is a supported image."));
    return;
  }
  if (!rejectedFiles.isEmpty()) {
    QMessageBox::warning(this, tr("Some Images Were Skipped"),
                         tr("These files could not be opened:\n%1").arg(rejectedFiles.join('\n')));
  }

  QSettings settings;
  settings.setValue(QString::fromLatin1(lastOpenDirectoryKey),
                    QFileInfo{fileNames.constFirst()}.absolutePath());
  settings.sync();
}

} // namespace quickshot
