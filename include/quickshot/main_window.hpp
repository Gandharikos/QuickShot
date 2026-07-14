#pragma once

#include <QMainWindow>

namespace quickshot {

class QDrawWidget;

class MainWindow final : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget* parent = nullptr);

private:
  void openImage();

  QDrawWidget* drawWidget_;
};

} // namespace quickshot
