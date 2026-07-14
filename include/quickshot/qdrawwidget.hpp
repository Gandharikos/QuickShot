#pragma once

#include <QAbstractScrollArea>
#include <QImage>

class QPaintEvent;
class QResizeEvent;
class QString;

namespace quickshot {

class QDrawWidget final : public QAbstractScrollArea {
  Q_OBJECT

public:
  explicit QDrawWidget(QWidget* parent = nullptr);

  [[nodiscard]] bool loadImage(const QString& fileName);
  [[nodiscard]] bool hasImage() const noexcept;
  [[nodiscard]] QSize sizeHint() const override;

protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

private:
  void updateScrollBars();

  QImage image_;
};

} // namespace quickshot
