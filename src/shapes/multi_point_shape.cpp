#include "quickshot/shapes/multi_point_shape.hpp"

#include <QPainter>
#include <QTransform>
#include <QtGlobal>
#include <algorithm>
#include <memory>
#include <utility>

namespace quickshot {
namespace {

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

bool hasPointArea(const QPolygonF& points) {
  if (points.size() < 3) {
    return false;
  }

  qreal doubledArea = 0.0;
  for (qsizetype index = 0; index < points.size(); ++index) {
    const QPointF& current = points[index];
    const QPointF& next = points[(index + 1) % points.size()];
    doubledArea += current.x() * next.y() - next.x() * current.y();
  }
  return !qFuzzyIsNull(doubledArea);
}

} // namespace

MultiPointShape::Geometry::Geometry(QPolygonF points, qreal rotationDegrees, bool closed,
                                    const QRectF& bounds)
    : points(std::move(points)), rotationDegrees(rotationDegrees), closed(closed), bounds(bounds) {}

bool MultiPointShape::Geometry::equals(const ShapeGeometry& other) const noexcept {
  const auto* geometry = dynamic_cast<const Geometry*>(&other);
  return geometry != nullptr && points == geometry->points && closed == geometry->closed &&
         qFuzzyCompare(rotationDegrees + 1.0, geometry->rotationDegrees + 1.0);
}

MultiPointShape::MultiPointShape(const QRectF& bounds) {
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

MultiPointShape::MultiPointShape(QPolygonF points) : points_(std::move(points)) {
  closed_ = hasPointArea(points_);
  rebuildHandles();
}

void MultiPointShape::draw(QPainter& painter) const { painter.drawPath(path()); }

QRectF MultiPointShape::boundingRect() const { return localPath().boundingRect(); }

void MultiPointShape::setBoundingRect(const QRectF& bounds) {
  if (points_.empty()) {
    return;
  }

  const QRectF source = boundingRect();
  const QRectF target = bounds.normalized();
  if (!qFuzzyIsNull(source.width()) && !qFuzzyIsNull(source.height())) {
    QTransform transformation;
    transformation.translate(target.left(), target.top());
    transformation.scale(target.width() / source.width(), target.height() / source.height());
    transformation.translate(-source.left(), -source.top());
    points_ = transformation.map(points_);
    if (previewPoint_.has_value()) {
      previewPoint_ = transformation.map(*previewPoint_);
    }
  } else {
    for (QPointF& point : points_) {
      point = mapPointBetweenBounds(point, source, target);
    }
    if (previewPoint_.has_value()) {
      previewPoint_ = mapPointBetweenBounds(*previewPoint_, source, target);
    }
  }
}

QPainterPath MultiPointShape::path() const { return mapPathToImage(localPath()); }

std::span<const ShapeHandle> MultiPointShape::handles() const noexcept { return handles_; }

QPointF MultiPointShape::handleCenter(const ShapeHandle& handle) const {
  const auto index = static_cast<qsizetype>(handle.id());
  Q_ASSERT(index < points_.size());
  return index < points_.size() ? mapToImage(points_[index]) : QPointF{};
}

std::unique_ptr<ShapeGeometry> MultiPointShape::captureGeometry() const {
  return std::make_unique<Geometry>(points_, rotationDegrees(), closed_, boundingRect());
}

void MultiPointShape::restoreGeometry(const ShapeGeometry& geometry) {
  const auto* pointGeometry = dynamic_cast<const Geometry*>(&geometry);
  Q_ASSERT(pointGeometry != nullptr);
  if (pointGeometry == nullptr) {
    return;
  }

  points_ = pointGeometry->points;
  closed_ = pointGeometry->closed;
  previewPoint_.reset();
  setRotationDegrees(pointGeometry->rotationDegrees);
  rebuildHandles();
}

void MultiPointShape::resize(const ShapeGeometry& initialGeometry, const ShapeHandle& handle,
                             const QPointF& imagePoint, const QRectF& imageBounds) {
  const auto* geometry = dynamic_cast<const Geometry*>(&initialGeometry);
  Q_ASSERT(geometry != nullptr);
  const auto index = static_cast<qsizetype>(handle.id());
  if (geometry == nullptr || index >= geometry->points.size()) {
    return;
  }

  points_ = geometry->points;
  closed_ = geometry->closed;
  previewPoint_.reset();
  const QPointF boundedPoint{std::clamp(imagePoint.x(), imageBounds.left(), imageBounds.right()),
                             std::clamp(imagePoint.y(), imageBounds.top(), imageBounds.bottom())};
  const QTransform initialTransform =
      rotationTransform(geometry->bounds, geometry->rotationDegrees);
  points_[index] = initialTransform.inverted().map(boundedPoint);
  const QTransform resizedTransform = rotationTransform(boundingRect(), geometry->rotationDegrees);
  const QPointF correction = boundedPoint - resizedTransform.map(points_[index]);
  points_.translate(correction);
  setRotationDegrees(geometry->rotationDegrees);
  rebuildHandles();
}

CreationKind MultiPointShape::creationKind() const noexcept { return CreationKind::MultiPoint; }

bool MultiPointShape::isCreationComplete() const noexcept { return closed_; }

void MultiPointShape::appendPoint(const QPointF& point) {
  if (closed_ || (!points_.empty() && points_.back() == point)) {
    return;
  }
  points_.push_back(point);
  previewPoint_ = point;
  rebuildHandles();
}

void MultiPointShape::setPreviewPoint(const QPointF& point) {
  if (!closed_) {
    previewPoint_ = point;
  }
}

void MultiPointShape::finishCreation() {
  previewPoint_.reset();
  closed_ = hasPointArea(points_);
}

qsizetype MultiPointShape::pointCount() const noexcept { return points_.size(); }

const QPolygonF& MultiPointShape::points() const noexcept { return points_; }

const std::optional<QPointF>& MultiPointShape::previewPoint() const noexcept {
  return previewPoint_;
}

bool MultiPointShape::isClosed() const noexcept { return closed_; }

void MultiPointShape::rebuildHandles() {
  handles_.clear();
  handles_.reserve(static_cast<std::size_t>(points_.size()));
  for (qsizetype index = 0; index < points_.size(); ++index) {
    handles_.emplace_back(static_cast<ShapeHandle::Id>(index), Qt::CrossCursor);
  }
}

} // namespace quickshot
