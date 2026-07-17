#pragma once

#include <QImage>
#include <QPainterPath>

class QString;

namespace quickshot {

class Shape;

[[nodiscard]] bool isRoiWithinImage(const QImage& image, const Shape& shape);
[[nodiscard]] QImage extractRoi(const QImage& image, const Shape& shape);
[[nodiscard]] bool saveRoiPng(const QImage& image, const Shape& shape, const QString& fileName,
                              QString* errorMessage = nullptr);
[[nodiscard]] bool isRoiWithinImage(const QImage& image, const QPainterPath& path);
[[nodiscard]] QImage extractRoi(const QImage& image, const QPainterPath& path);
[[nodiscard]] bool saveRoiPng(const QImage& image, const QPainterPath& path,
                              const QString& fileName, QString* errorMessage = nullptr);

} // namespace quickshot
