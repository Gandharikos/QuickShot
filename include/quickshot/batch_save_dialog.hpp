#pragma once

#include <QDialog>
#include <QString>
#include <vector>

class QLineEdit;
class QWidget;

namespace quickshot {

struct BatchSaveRow {
  QString imagePath;
  bool savable;
  QString statusMessage;
};

class BatchSaveDialog final : public QDialog {
public:
  BatchSaveDialog(const std::vector<BatchSaveRow>& rows, const QString& initialDirectory,
                  QWidget* parent = nullptr);

  [[nodiscard]] QString outputDirectory() const;

private:
  QLineEdit* outputDirectoryEdit_;
};

} // namespace quickshot
