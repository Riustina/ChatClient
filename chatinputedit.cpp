#include "chatinputedit.h"

#include <QMimeData>

ChatInputEdit::ChatInputEdit(QWidget *parent)
    : QTextEdit(parent)
{
}

bool ChatInputEdit::hasPendingImage() const
{
    return !_pendingImage.isNull();
}

QImage ChatInputEdit::takePastedImage()
{
    QImage image = _pendingImage;
    _pendingImage = QImage();
    return image;
}

void ChatInputEdit::insertFromMimeData(const QMimeData *source)
{
    if (source->hasImage()) {
        const QVariant imageData = source->imageData();
        if (imageData.canConvert<QImage>()) {
            _pendingImage = qvariant_cast<QImage>(imageData);
            emit imagePasted();
            return;
        }
    }

    QTextEdit::insertFromMimeData(source);
}
