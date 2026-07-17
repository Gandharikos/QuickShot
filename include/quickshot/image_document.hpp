#pragma once

#include <QImage>
#include <QString>
#include <QUndoStack>
#include <memory>

namespace quickshot {

class ImageScene;

class ImageDocument final {
public:
  ImageDocument(QString filePath, QImage image);
  ~ImageDocument();

  ImageDocument(const ImageDocument&) = delete;
  ImageDocument& operator=(const ImageDocument&) = delete;
  ImageDocument(ImageDocument&&) = delete;
  ImageDocument& operator=(ImageDocument&&) = delete;

  [[nodiscard]] const QString& filePath() const noexcept;
  [[nodiscard]] const QImage& image() const noexcept;
  void setImage(QImage image);
  [[nodiscard]] ImageScene& scene() noexcept;
  [[nodiscard]] const ImageScene& scene() const noexcept;
  [[nodiscard]] QUndoStack& undoStack() noexcept;

private:
  QString filePath_;
  QImage image_;
  // Member destruction is reversed: commands release detached items while the scene still exists.
  std::unique_ptr<ImageScene> scene_;
  QUndoStack undoStack_;
};

} // namespace quickshot
