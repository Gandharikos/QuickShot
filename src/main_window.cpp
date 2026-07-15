#include "quickshot/main_window.hpp"

#include "quickshot/qdrawwidget.hpp"
#include "quickshot/shape.hpp"

#include <QAction>
#include <QActionGroup>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QImageReader>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QSvgRenderer>
#include <QToolBar>

namespace quickshot {

namespace {

constexpr auto lastOpenDirectoryKey = "image/lastOpenDirectory";

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

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), drawWidget_(new QDrawWidget(this)) {
  setWindowTitle(tr("Quickshot"));
  resize(720, 420);

  auto* toolbar = addToolBar(tr("File"));
  auto* openButton = new QPushButton(tr("Open"), toolbar);

  toolbar->setObjectName("mainToolBar");
  toolbar->setMovable(false);
  toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
  openButton->setObjectName("openButton");
  drawWidget_->setObjectName("drawWidget");

  toolbar->addWidget(openButton);
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
  rectangleAction->setObjectName("rectangleAction");
  ellipseAction->setObjectName("ellipseAction");
  rectangleAction->setCheckable(true);
  ellipseAction->setCheckable(true);
  rectangleAction->setEnabled(false);
  ellipseAction->setEnabled(false);
  shapeActions->addAction(rectangleAction);
  shapeActions->addAction(ellipseAction);

  connect(openButton, &QPushButton::clicked, this, &MainWindow::openImage);
  connect(rotateLeftAction, &QAction::triggered, drawWidget_, &QDrawWidget::rotateLeft);
  connect(rotateRightAction, &QAction::triggered, drawWidget_, &QDrawWidget::rotateRight);
  connect(zoomFactorSpinBox, &QDoubleSpinBox::valueChanged, drawWidget_,
          &QDrawWidget::setZoomFactor);
  connect(drawWidget_, &QDrawWidget::zoomFactorChanged, zoomFactorSpinBox,
          &QDoubleSpinBox::setValue);
  connect(rectangleAction, &QAction::triggered, this,
          [this](bool enabled) { drawWidget_->setCreationMode(ShapeType::Rectangle, enabled); });
  connect(ellipseAction, &QAction::triggered, this,
          [this](bool enabled) { drawWidget_->setCreationMode(ShapeType::Ellipse, enabled); });
  connect(drawWidget_, &QDrawWidget::imageAvailabilityChanged, rotateLeftAction,
          &QAction::setEnabled);
  connect(drawWidget_, &QDrawWidget::imageAvailabilityChanged, rotateRightAction,
          &QAction::setEnabled);
  connect(drawWidget_, &QDrawWidget::imageAvailabilityChanged, zoomFactorSpinBox,
          &QDoubleSpinBox::setEnabled);
  connect(drawWidget_, &QDrawWidget::imageAvailabilityChanged, rectangleAction,
          &QAction::setEnabled);
  connect(drawWidget_, &QDrawWidget::imageAvailabilityChanged, ellipseAction, &QAction::setEnabled);

  setCentralWidget(drawWidget_);
}

void MainWindow::openImage() {
  const QString fileName =
      QFileDialog::getOpenFileName(this, tr("Open Image"), lastOpenDirectory(), imageFilter());
  if (fileName.isEmpty()) {
    return;
  }

  if (!drawWidget_->loadImage(fileName)) {
    QMessageBox::warning(this, tr("Invalid Image"),
                         tr("The selected file is not a supported image."));
    return;
  }

  QSettings{}.setValue(QString::fromLatin1(lastOpenDirectoryKey),
                       QFileInfo{fileName}.absolutePath());
}

} // namespace quickshot
