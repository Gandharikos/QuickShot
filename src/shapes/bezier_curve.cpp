#include "quickshot/shapes/bezier_curve.hpp"

#include <memory>
#include <utility>

namespace quickshot {
namespace {

constexpr qreal controlPointScale = 1.0 / 6.0;

void appendBezierSegment(QPainterPath& path, const QPointF& previous, const QPointF& current,
                         const QPointF& next, const QPointF& following) {
  const QPointF firstControl = current + (next - previous) * controlPointScale;
  const QPointF secondControl = next - (following - current) * controlPointScale;
  path.cubicTo(firstControl, secondControl, next);
}

QPainterPath openBezierPath(const QPolygonF& anchors) {
  QPainterPath path;
  if (anchors.empty()) {
    return path;
  }

  path.moveTo(anchors.front());
  for (qsizetype index = 0; index + 1 < anchors.size(); ++index) {
    const QPointF& previous = index == 0 ? anchors[index] : anchors[index - 1];
    const QPointF& current = anchors[index];
    const QPointF& next = anchors[index + 1];
    const QPointF& following = index + 2 < anchors.size() ? anchors[index + 2] : anchors[index + 1];
    appendBezierSegment(path, previous, current, next, following);
  }
  return path;
}

QPainterPath closedBezierPath(const QPolygonF& anchors) {
  QPainterPath path;
  if (anchors.size() < 3) {
    return path;
  }

  path.moveTo(anchors.front());
  for (qsizetype index = 0; index < anchors.size(); ++index) {
    const qsizetype previousIndex = (index + anchors.size() - 1) % anchors.size();
    const qsizetype nextIndex = (index + 1) % anchors.size();
    const qsizetype followingIndex = (index + 2) % anchors.size();
    appendBezierSegment(path, anchors[previousIndex], anchors[index], anchors[nextIndex],
                        anchors[followingIndex]);
  }
  path.closeSubpath();
  return path;
}

} // namespace

BezierCurve::BezierCurve(const QRectF& bounds) : MultiPointShape(bounds) {
  const QRectF normalizedBounds = bounds.normalized();
  if (!normalizedBounds.isEmpty()) {
    setBoundingRect(normalizedBounds);
  }
}

BezierCurve::BezierCurve(QPolygonF points) : MultiPointShape(std::move(points)) {}

std::unique_ptr<Shape> BezierCurve::clone() const {
  auto curve = std::make_unique<BezierCurve>(points());
  curve->setRotationDegrees(rotationDegrees());
  return curve;
}

QPainterPath BezierCurve::localPath() const {
  if (isClosed()) {
    return closedBezierPath(points());
  }

  QPolygonF anchors = points();
  if (previewPoint().has_value() && (anchors.empty() || anchors.back() != *previewPoint())) {
    anchors.push_back(*previewPoint());
  }
  return openBezierPath(anchors);
}

} // namespace quickshot
