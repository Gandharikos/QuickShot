#include "quickshot/roi_exporter.hpp"

#include "quickshot/shape.hpp"

#include <QImageWriter>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QRect>
#include <QString>
#include <QTransform>

namespace quickshot {

QImage extractRoi(const QImage& image, const Shape& shape) {
  const QRect pixelBounds = shape.path().boundingRect().toAlignedRect().intersected(image.rect());
  if (image.isNull() || pixelBounds.isEmpty()) {
    return {};
  }

  QImage roi{pixelBounds.size(), QImage::Format_ARGB32_Premultiplied};
  roi.fill(Qt::transparent);

  QTransform toRoiCoordinates;
  toRoiCoordinates.translate(-static_cast<qreal>(pixelBounds.x()),
                             -static_cast<qreal>(pixelBounds.y()));

  QPainter painter{&roi};
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setClipPath(toRoiCoordinates.map(shape.path()));
  painter.drawImage(
      QPointF{-static_cast<qreal>(pixelBounds.x()), -static_cast<qreal>(pixelBounds.y())}, image);
  return roi;
}

bool saveRoiPng(const QImage& image, const Shape& shape, const QString& fileName,
                QString* errorMessage) {
  const QImage roi = extractRoi(image, shape);
  if (roi.isNull()) {
    if (errorMessage != nullptr) {
      *errorMessage = QStringLiteral("The ROI is outside the image or has no area.");
    }
    return false;
  }

  QImageWriter writer{fileName, "png"};
  if (writer.write(roi)) {
    return true;
  }

  if (errorMessage != nullptr) {
    *errorMessage = writer.errorString();
  }
  return false;
}

} // namespace quickshot
