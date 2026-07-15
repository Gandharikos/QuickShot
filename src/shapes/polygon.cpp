#include "quickshot/shapes/polygon.hpp"

#include <QPainter>
#include <QTransform>
#include <QtGlobal>
#include <algorithm>
#include <memory>
#include <utility>

namespace quickshot {
namespace {

QRectF pointBounds(const std::vector<QPointF>& points) {
  if (points.empty()) {
    return {};
  }

  qreal left = points.front().x();
  qreal right = left;
  qreal top = points.front().y();
  qreal bottom = top;
  for (const QPointF& point : points) {
    left = std::min(left, point.x());
    right = std::max(right, point.x());
    top = std::min(top, point.y());
    bottom = std::max(bottom, point.y());
  }
  return {QPointF{left, top}, QPointF{right, bottom}};
}

QPointF mapPointBetweenBounds(const QPointF& point, const QRectF& source, const QRectF& target) {
  const qreal x =
      qFuzzyIsNull(source.width())
          ? target.center().x()
          : target.left() + (point.x() - source.left()) * target.width() / source.width();
  const qreal y =
      qFuzzyIsNull(source.height())
          ? target.center().y()
          : target.top() + (point.y() - source.top()) * target.height() / source.height();
  return {x, y};
}

QTransform rotationTransform(const QRectF& bounds, qreal degrees) {
  const QPointF center = bounds.center();
  QTransform transformation;
  transformation.translate(center.x(), center.y());
  transformation.rotate(degrees);
  transformation.translate(-center.x(), -center.y());
  return transformation;
}

bool hasPolygonArea(const std::vector<QPointF>& points) {
  if (points.size() < 3) {
    return false;
  }

  qreal doubledArea = 0.0;
  for (std::size_t index = 0; index < points.size(); ++index) {
    const QPointF& current = points[index];
    const QPointF& next = points[(index + 1) % points.size()];
    doubledArea += current.x() * next.y() - next.x() * current.y();
  }
  return !qFuzzyIsNull(doubledArea);
}

} // namespace

Polygon::Geometry::Geometry(std::vector<QPointF> points, qreal rotationDegrees, bool closed)
    : points(std::move(points)), rotationDegrees(rotationDegrees), closed(closed) {}

bool Polygon::Geometry::equals(const ShapeGeometry& other) const noexcept {
  const auto* geometry = dynamic_cast<const Geometry*>(&other);
  return geometry != nullptr && points == geometry->points && closed == geometry->closed &&
         qFuzzyCompare(rotationDegrees + 1.0, geometry->rotationDegrees + 1.0);
}

Polygon::Polygon(const QRectF& bounds) {
  const QRectF normalizedBounds = bounds.normalized();
  if (normalizedBounds.isEmpty()) {
    points_.push_back(normalizedBounds.topLeft());
  } else {
    points_ = {normalizedBounds.topLeft(), normalizedBounds.topRight(),
               normalizedBounds.bottomRight(), normalizedBounds.bottomLeft()};
    closed_ = true;
  }
  rebuildHandles();
}

Polygon::Polygon(std::vector<QPointF> points) : points_(std::move(points)) {
  closed_ = hasPolygonArea(points_);
  rebuildHandles();
}

std::unique_ptr<Shape> Polygon::clone() const {
  auto polygon = std::make_unique<Polygon>(points_);
  polygon->closed_ = closed_;
  polygon->previewPoint_ = previewPoint_;
  polygon->setRotationDegrees(rotationDegrees());
  return polygon;
}

void Polygon::draw(QPainter& painter) const { painter.drawPath(path()); }

QRectF Polygon::boundingRect() const { return pointBounds(points_); }

void Polygon::setBoundingRect(const QRectF& bounds) {
  if (points_.empty()) {
    return;
  }

  const QRectF source = boundingRect();
  const QRectF target = bounds.normalized();
  for (QPointF& point : points_) {
    point = mapPointBetweenBounds(point, source, target);
  }
  if (previewPoint_.has_value()) {
    previewPoint_ = mapPointBetweenBounds(*previewPoint_, source, target);
  }
}

QPainterPath Polygon::path() const {
  QPainterPath shapePath;
  if (points_.empty()) {
    return shapePath;
  }

  shapePath.moveTo(points_.front());
  for (std::size_t index = 1; index < points_.size(); ++index) {
    shapePath.lineTo(points_[index]);
  }
  if (closed_) {
    shapePath.closeSubpath();
  } else if (previewPoint_.has_value()) {
    shapePath.lineTo(*previewPoint_);
  }
  return mapPathToImage(shapePath);
}

std::span<const ShapeHandle> Polygon::handles() const noexcept { return handles_; }

QPointF Polygon::handleCenter(const ShapeHandle& handle) const {
  const std::size_t index = static_cast<std::size_t>(handle.id());
  Q_ASSERT(index < points_.size());
  return index < points_.size() ? mapToImage(points_[index]) : QPointF{};
}

std::unique_ptr<ShapeGeometry> Polygon::captureGeometry() const {
  return std::make_unique<Geometry>(points_, rotationDegrees(), closed_);
}

void Polygon::restoreGeometry(const ShapeGeometry& geometry) {
  const auto* polygonGeometry = dynamic_cast<const Geometry*>(&geometry);
  Q_ASSERT(polygonGeometry != nullptr);
  if (polygonGeometry == nullptr) {
    return;
  }

  points_ = polygonGeometry->points;
  closed_ = polygonGeometry->closed;
  previewPoint_.reset();
  setRotationDegrees(polygonGeometry->rotationDegrees);
  rebuildHandles();
}

void Polygon::resize(const ShapeGeometry& initialGeometry, const ShapeHandle& handle,
                     const QPointF& imagePoint, const QRectF& imageBounds) {
  const auto* geometry = dynamic_cast<const Geometry*>(&initialGeometry);
  Q_ASSERT(geometry != nullptr);
  const std::size_t index = static_cast<std::size_t>(handle.id());
  if (geometry == nullptr || index >= geometry->points.size()) {
    return;
  }

  points_ = geometry->points;
  const QPointF boundedPoint{std::clamp(imagePoint.x(), imageBounds.left(), imageBounds.right()),
                             std::clamp(imagePoint.y(), imageBounds.top(), imageBounds.bottom())};
  const QTransform initialTransform =
      rotationTransform(pointBounds(geometry->points), geometry->rotationDegrees);
  points_[index] = initialTransform.inverted().map(boundedPoint);
  const QTransform resizedTransform =
      rotationTransform(pointBounds(points_), geometry->rotationDegrees);
  const QPointF correction = boundedPoint - resizedTransform.map(points_[index]);
  for (QPointF& point : points_) {
    point += correction;
  }
  closed_ = geometry->closed;
  previewPoint_.reset();
  setRotationDegrees(geometry->rotationDegrees);
  rebuildHandles();
}

CreationKind Polygon::creationKind() const noexcept { return CreationKind::MultiPoint; }

bool Polygon::isCreationComplete() const noexcept { return closed_; }

void Polygon::appendPoint(const QPointF& point) {
  if (closed_ || (!points_.empty() && points_.back() == point)) {
    return;
  }
  points_.push_back(point);
  previewPoint_ = point;
  rebuildHandles();
}

void Polygon::setPreviewPoint(const QPointF& point) {
  if (!closed_) {
    previewPoint_ = point;
  }
}

void Polygon::finishCreation() {
  previewPoint_.reset();
  closed_ = hasPolygonArea(points_);
}

std::size_t Polygon::pointCount() const noexcept { return points_.size(); }

void Polygon::rebuildHandles() {
  handles_.clear();
  handles_.reserve(points_.size());
  for (std::size_t index = 0; index < points_.size(); ++index) {
    handles_.emplace_back(static_cast<ShapeHandle::Id>(index), Qt::CrossCursor);
  }
}

} // namespace quickshot
