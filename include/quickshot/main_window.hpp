#pragma once

#include <QMainWindow>

namespace quickshot {

class CanvasView;

class MainWindow final : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget* parent = nullptr);

private:
  void openImage();

  CanvasView* canvasView_;
};

} // namespace quickshot
