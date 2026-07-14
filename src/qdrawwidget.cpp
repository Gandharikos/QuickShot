#include "quickshot/qdrawwidget.hpp"

#include <QFrame>
#include <QImageReader>
#include <QPainter>
#include <QPoint>
#include <QRectF>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSize>
#include <algorithm>
#include <utility>

namespace quickshot {

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
  updateScrollBars();
  viewport()->update();
  return true;
}

bool QDrawWidget::hasImage() const noexcept { return !image_.isNull(); }

QSize QDrawWidget::sizeHint() const { return {640, 360}; }

void QDrawWidget::paintEvent(QPaintEvent* event) {
  QAbstractScrollArea::paintEvent(event);

  QPainter painter(viewport());
  const QRectF viewportRect{0.0, 0.0, static_cast<qreal>(viewport()->width()),
                            static_cast<qreal>(viewport()->height())};
  // The image stays at its native size, so uncovered viewport pixels remain black.
  painter.fillRect(viewportRect, Qt::black);

  if (image_.isNull()) {
    return;
  }

  // Scrollbar values are content offsets; negate them to map the image into viewport coordinates.
  const QPoint imagePosition{-horizontalScrollBar()->value(), -verticalScrollBar()->value()};
  painter.drawImage(imagePosition, image_);
}

void QDrawWidget::resizeEvent(QResizeEvent* event) {
  QAbstractScrollArea::resizeEvent(event);
  // A resized viewport changes the visible page and therefore the valid scroll range.
  updateScrollBars();
}

void QDrawWidget::updateScrollBars() {
  const QSize pageSize = viewport()->size();
  const QSize imageSize = image_.size();
  const int horizontalMaximum = std::max(0, imageSize.width() - pageSize.width());
  const int verticalMaximum = std::max(0, imageSize.height() - pageSize.height());

  // The page step is the visible extent; a zero maximum lets Qt hide an unnecessary scrollbar.
  horizontalScrollBar()->setPageStep(pageSize.width());
  horizontalScrollBar()->setRange(0, horizontalMaximum);

  verticalScrollBar()->setPageStep(pageSize.height());
  verticalScrollBar()->setRange(0, verticalMaximum);
}

} // namespace quickshot
