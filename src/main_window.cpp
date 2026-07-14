#include "quickshot/main_window.hpp"

#include "quickshot/qdrawwidget.hpp"

#include <QFileDialog>
#include <QImageReader>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>
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

  connect(openButton, &QPushButton::clicked, this, &MainWindow::openImage);

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
