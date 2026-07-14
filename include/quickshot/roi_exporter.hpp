#pragma once

#include <QImage>

class QString;

namespace quickshot {

class Shape;

[[nodiscard]] QImage extractRoi(const QImage& image, const Shape& shape);
[[nodiscard]] bool saveRoiPng(const QImage& image, const Shape& shape, const QString& fileName,
                              QString* errorMessage = nullptr);

} // namespace quickshot
