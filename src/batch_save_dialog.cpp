#include "quickshot/batch_save_dialog.hpp"

#include <QAbstractItemView>
#include <QColor>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <algorithm>

namespace quickshot {

BatchSaveDialog::BatchSaveDialog(const std::vector<BatchSaveRow>& rows,
                                 const QString& initialDirectory, QWidget* parent)
    : QDialog(parent), outputDirectoryEdit_(new QLineEdit(initialDirectory, this)) {
  setWindowTitle(tr("Batch Save"));
  setObjectName("batchSaveDialog");
  resize(760, 420);

  auto* browseButton = new QPushButton(tr("Browse…"), this);
  auto* pathLayout = new QHBoxLayout;
  outputDirectoryEdit_->setObjectName("batchOutputDirectoryEdit");
  browseButton->setObjectName("batchBrowseButton");
  pathLayout->addWidget(outputDirectoryEdit_);
  pathLayout->addWidget(browseButton);

  auto* table = new QTableWidget(static_cast<int>(rows.size()), 2, this);
  table->setObjectName("batchSaveTable");
  table->setHorizontalHeaderLabels({tr("Image"), tr("Status")});
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->setSelectionMode(QAbstractItemView::NoSelection);
  table->setAlternatingRowColors(true);
  table->verticalHeader()->hide();
  table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

  for (std::size_t index = 0; index < rows.size(); ++index) {
    const BatchSaveRow& row = rows[index];
    const int tableRow = static_cast<int>(index);
    auto* imageItem = new QTableWidgetItem{
        QStringLiteral("%1\n%2").arg(QFileInfo{row.imagePath}.fileName(), row.imagePath)};
    imageItem->setToolTip(row.imagePath);
    auto* statusItem =
        new QTableWidgetItem{row.savable ? QStringLiteral("✓") : QStringLiteral("✗")};
    QFont statusFont = statusItem->font();
    statusFont.setBold(true);
    statusFont.setPointSize(statusFont.pointSize() + 4);
    statusItem->setFont(statusFont);
    statusItem->setForeground(row.savable ? QColor{0, 150, 70} : QColor{200, 45, 45});
    statusItem->setTextAlignment(Qt::AlignCenter);
    statusItem->setToolTip(row.statusMessage);
    table->setItem(tableRow, 0, imageItem);
    table->setItem(tableRow, 1, statusItem);
    table->setRowHeight(tableRow, 52);
  }

  auto* buttons = new QDialogButtonBox{QDialogButtonBox::Save | QDialogButtonBox::Cancel, this};
  QPushButton* saveButton = buttons->button(QDialogButtonBox::Save);
  saveButton->setText(tr("Save Valid Images"));
  const bool hasSavableRows =
      std::ranges::any_of(rows, [](const BatchSaveRow& row) { return row.savable; });
  saveButton->setEnabled(hasSavableRows && QDir{initialDirectory}.exists());

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(new QLabel{tr("Output directory:"), this});
  layout->addLayout(pathLayout);
  layout->addWidget(table);
  layout->addWidget(buttons);

  connect(browseButton, &QPushButton::clicked, this, [this]() {
    const QString selectedDirectory = QFileDialog::getExistingDirectory(
        this, tr("Select Output Directory"), outputDirectoryEdit_->text());
    if (!selectedDirectory.isEmpty()) {
      outputDirectoryEdit_->setText(selectedDirectory);
    }
  });
  connect(outputDirectoryEdit_, &QLineEdit::textChanged, saveButton,
          [saveButton, hasSavableRows](const QString& directory) {
            saveButton->setEnabled(hasSavableRows && QDir{directory}.exists());
          });
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString BatchSaveDialog::outputDirectory() const { return outputDirectoryEdit_->text(); }

} // namespace quickshot
