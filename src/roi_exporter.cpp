#include "quickshot/roi_exporter.hpp"

#include "quickshot/shapes/shape.hpp"

#include <QImageWriter>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QRect>
#include <QString>
#include <QTransform>

namespace quickshot {

bool isRoiWithinImage(const QImage& image, const Shape& shape) {
  return isRoiWithinImage(image, shape.path());
}

bool isRoiWithinImage(const QImage& image, const QPainterPath& path) {
  if (image.isNull()) {
    return false;
  }
  const QRect pixelBounds = path.boundingRect().toAlignedRect();
  return !pixelBounds.isEmpty() && image.rect().contains(pixelBounds);
}

QImage extractRoi(const QImage& image, const Shape& shape) {
  return extractRoi(image, shape.path());
}

QImage extractRoi(const QImage& image, const QPainterPath& path) {
  const QRect pixelBounds = path.boundingRect().toAlignedRect().intersected(image.rect());
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
  painter.setClipPath(toRoiCoordinates.map(path));
  painter.drawImage(
      QPointF{-static_cast<qreal>(pixelBounds.x()), -static_cast<qreal>(pixelBounds.y())}, image);
  return roi;
}

bool saveRoiPng(const QImage& image, const Shape& shape, const QString& fileName,
                QString* errorMessage) {
  return saveRoiPng(image, shape.path(), fileName, errorMessage);
}

bool saveRoiPng(const QImage& image, const QPainterPath& path, const QString& fileName,
                QString* errorMessage) {
  const QImage roi = extractRoi(image, path);
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
