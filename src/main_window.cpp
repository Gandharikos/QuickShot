#include "quickshot/main_window.hpp"

#include "quickshot/qdrawwidget.hpp"

#include <QAction>
#include <QFileDialog>
#include <QIcon>
#include <QImageReader>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QStringList>
#include <QSvgRenderer>
#include <QToolBar>

namespace quickshot {

namespace {

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

  connect(openButton, &QPushButton::clicked, this, &MainWindow::openImage);
  connect(rotateLeftAction, &QAction::triggered, drawWidget_, &QDrawWidget::rotateLeft);
  connect(rotateRightAction, &QAction::triggered, drawWidget_, &QDrawWidget::rotateRight);
  connect(drawWidget_, &QDrawWidget::imageAvailabilityChanged, rotateLeftAction,
          &QAction::setEnabled);
  connect(drawWidget_, &QDrawWidget::imageAvailabilityChanged, rotateRightAction,
          &QAction::setEnabled);

  setCentralWidget(drawWidget_);
}

void MainWindow::openImage() {
  const QString fileName = QFileDialog::getOpenFileName(this, tr("Open Image"), {}, imageFilter());
  if (fileName.isEmpty()) {
    return;
  }

  if (!drawWidget_->loadImage(fileName)) {
    QMessageBox::warning(this, tr("Invalid Image"),
                         tr("The selected file is not a supported image."));
  }
}

} // namespace quickshot
