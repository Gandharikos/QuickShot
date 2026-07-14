#include "quickshot/qdrawwidget.hpp"

#include <QFrame>
#include <QImageReader>
#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSize>
#include <QTransform>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <utility>

namespace quickshot {
namespace {

constexpr qreal minimumZoom = 0.1;
constexpr qreal maximumZoom = 8.0;
constexpr qreal zoomPerWheelStep = 1.1;
constexpr qreal wheelStepAngle = 120.0;

} // namespace

QDrawWidget::QDrawWidget(QWidget* parent) : QAbstractScrollArea(parent) {
  setFrameShape(QFrame::NoFrame);
  viewport()->setAutoFillBackground(false);
}

bool QDrawWidget::loadImage(const QString& fileName) {
  QImageReader reader(fileName);
  reader.setAutoTransform(true);

  QImage image = reader.read();
  if (image.isNull()) {
    return false;
  }

  image_ = std::move(image);
  zoomFactor_ = 1.0;
  horizontalScrollBar()->setValue(0);
  verticalScrollBar()->setValue(0);
  updateScrollBars();
  viewport()->update();
  emit imageAvailabilityChanged(true);
  return true;
}

bool QDrawWidget::hasImage() const noexcept { return !image_.isNull(); }

qreal QDrawWidget::zoomFactor() const noexcept { return zoomFactor_; }

QSize QDrawWidget::sizeHint() const { return {640, 360}; }

void QDrawWidget::rotateLeft() { rotateImage(-90.0); }

void QDrawWidget::rotateRight() { rotateImage(90.0); }

void QDrawWidget::paintEvent(QPaintEvent* event) {
  QAbstractScrollArea::paintEvent(event);

  QPainter painter(viewport());
  const QRectF viewportRect{0.0, 0.0, static_cast<qreal>(viewport()->width()),
                            static_cast<qreal>(viewport()->height())};
  // Uncovered viewport pixels remain black at every zoom level.
  painter.fillRect(viewportRect, Qt::black);

  if (image_.isNull()) {
    return;
  }

  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  // Scrollbar values are offsets in scaled content coordinates, so translate in the opposite
  // direction before scaling the image uniformly.
  painter.translate(-horizontalScrollBar()->value(), -verticalScrollBar()->value());
  painter.scale(zoomFactor_, zoomFactor_);
  painter.drawImage(QPointF{0.0, 0.0}, image_);
}

void QDrawWidget::resizeEvent(QResizeEvent* event) {
  QAbstractScrollArea::resizeEvent(event);
  // A resized viewport changes the visible page and therefore the valid scroll range.
  updateScrollBars();
}

void QDrawWidget::wheelEvent(QWheelEvent* event) {
  // y() is vertical wheel rotation; x() carries horizontal input from tilt wheels or touchpads.
  const int angleDelta = event->angleDelta().y();
  if (image_.isNull() || !event->modifiers().testFlag(Qt::ControlModifier) || angleDelta == 0) {
    QAbstractScrollArea::wheelEvent(event);
    return;
  }

  const qreal previousZoom = zoomFactor_;
  // Qt reports angleDelta in eighths of a degree, so a typical 15-degree notch is 120 units;
  // keeping a fractional step also supports high-resolution wheels.
  const qreal wheelSteps = static_cast<qreal>(angleDelta) / wheelStepAngle;
  // Multiplication gives every notch the same relative visual change, while clamp keeps the
  // resulting scale within the supported 10%-800% range.
  zoomFactor_ =
      std::clamp(previousZoom * std::pow(zoomPerWheelStep, wheelSteps), minimumZoom, maximumZoom);

  // Compare floating-point values approximately; clamp can leave the scale unchanged at a limit.
  if (qFuzzyCompare(zoomFactor_, previousZoom)) {
    event->accept();
    return;
  }

  // Keeping the scrollbar offsets unchanged makes the image's top-left corner the zoom anchor.
  updateScrollBars();
  viewport()->update();
  event->accept();
}

void QDrawWidget::rotateImage(qreal degrees) {
  if (image_.isNull()) {
    return;
  }

  QTransform rotation;
  rotation.rotate(degrees);
  image_ = image_.transformed(rotation);
  updateScrollBars();
  viewport()->update();
}

QSize QDrawWidget::scaledImageSize() const {
  if (image_.isNull()) {
    return {};
  }

  return {std::max(1, qRound(static_cast<qreal>(image_.width()) * zoomFactor_)),
          std::max(1, qRound(static_cast<qreal>(image_.height()) * zoomFactor_))};
}

void QDrawWidget::updateScrollBars() {
  const QSize pageSize = viewport()->size();
  const QSize imageSize = scaledImageSize();
  const int horizontalMaximum = std::max(0, imageSize.width() - pageSize.width());
  const int verticalMaximum = std::max(0, imageSize.height() - pageSize.height());

  // The page step is the visible extent; a zero maximum lets Qt hide an unnecessary scrollbar.
  horizontalScrollBar()->setPageStep(pageSize.width());
  horizontalScrollBar()->setRange(0, horizontalMaximum);

  verticalScrollBar()->setPageStep(pageSize.height());
  verticalScrollBar()->setRange(0, verticalMaximum);
}

} // namespace quickshot
