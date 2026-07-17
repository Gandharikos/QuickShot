#include "quickshot/image_document.hpp"

#include "quickshot/image_scene.hpp"

#include <utility>

namespace quickshot {

ImageDocument::ImageDocument(QString filePath, QImage image)
    : filePath_(std::move(filePath)), image_(std::move(image)),
      scene_(std::make_unique<ImageScene>(image_)) {
  scene_->setUndoStack(undoStack_);
}

ImageDocument::~ImageDocument() = default;

const QString& ImageDocument::filePath() const noexcept { return filePath_; }

const QImage& ImageDocument::image() const noexcept { return image_; }

void ImageDocument::setImage(QImage image) {
  image_ = std::move(image);
  scene_->setImage(image_);
}

ImageScene& ImageDocument::scene() noexcept { return *scene_; }

const ImageScene& ImageDocument::scene() const noexcept { return *scene_; }

QUndoStack& ImageDocument::undoStack() noexcept { return undoStack_; }

} // namespace quickshot
