#include "chatinputedit.h"

#include <QKeyEvent>
#include <QMimeData>
#include <QPalette>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextImageFormat>
#include <QUrl>

ChatInputEdit::ChatInputEdit(QWidget *parent)
    : QTextEdit(parent)
{
    document()->setDocumentMargin(0);
    QPalette palette = this->palette();
    palette.setColor(QPalette::Base, QColor("#F4F3F9"));
    palette.setColor(QPalette::Text, QColor("#111827"));
    palette.setColor(QPalette::PlaceholderText, QColor("#6B7280"));
    palette.setColor(QPalette::Highlight, QColor("#DBEAFE"));
    palette.setColor(QPalette::HighlightedText, QColor("#111827"));
    setPalette(palette);
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

QString ChatInputEdit::plainTextForSend() const
{
    QString text = toPlainText();
    text.remove(QChar::ObjectReplacementCharacter);
    return text;
}

void ChatInputEdit::insertFromMimeData(const QMimeData *source)
{
    if (source->hasImage()) {
        const QVariant imageData = source->imageData();
        if (imageData.canConvert<QImage>()) {
            _pendingImage = qvariant_cast<QImage>(imageData);
            insertImagePreview(_pendingImage);
            emit imagePasted();
            return;
        }
    }

    QTextEdit::insertFromMimeData(source);
    syncPendingImageState();
}

void ChatInputEdit::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (event->modifiers() & Qt::ShiftModifier) {
            QTextEdit::keyPressEvent(event);
            syncPendingImageState();
            return;
        }

        emit sendRequested();
        event->accept();
        return;
    }

    QTextEdit::keyPressEvent(event);
    syncPendingImageState();
}

void ChatInputEdit::insertImagePreview(const QImage &image)
{
    if (image.isNull()) {
        return;
    }

    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End);

    const QString imageName = QStringLiteral("chat-preview-image");
    const QSize imageSize = image.size().scaled(180, 140, Qt::KeepAspectRatio);
    document()->addResource(QTextDocument::ImageResource, QUrl(imageName), image.scaled(imageSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    QTextImageFormat format;
    format.setName(imageName);
    format.setWidth(imageSize.width());
    format.setHeight(imageSize.height());

    cursor.insertImage(format);
    setTextCursor(cursor);
}

void ChatInputEdit::syncPendingImageState()
{
    if (_pendingImage.isNull()) {
        return;
    }

    if (!toPlainText().contains(QChar::ObjectReplacementCharacter)) {
        _pendingImage = QImage();
    }
}
