#pragma once

#include <QAbstractScrollArea>
#include <QImage>

class QPaintEvent;
class QResizeEvent;
class QString;
class QWheelEvent;

namespace quickshot {

class QDrawWidget final : public QAbstractScrollArea {
  Q_OBJECT

public:
  explicit QDrawWidget(QWidget* parent = nullptr);

  [[nodiscard]] bool loadImage(const QString& fileName);
  [[nodiscard]] bool hasImage() const noexcept;
  [[nodiscard]] qreal zoomFactor() const noexcept;
  [[nodiscard]] QSize sizeHint() const override;

protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

private:
  [[nodiscard]] QSize scaledImageSize() const;
  void updateScrollBars();

  QImage image_;
  qreal zoomFactor_ = 1.0;
};

} // namespace quickshot
