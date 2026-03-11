#include "messagecell.h"

#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QTextBrowser>
#include <QTextDocument>
#include <QtMath>

namespace {
constexpr int kAvatarSize = 34;
constexpr int kSideMargin = 8;
constexpr int kBubblePaddingH = 14;
constexpr int kBubblePaddingV = 8;
constexpr int kBubbleMaxWidth = 1875;

QPixmap buildAvatarPixmap(const QString &name, const QColor &color, const QSize &size)
{
    const qreal dpr = 2.0;
    QPixmap pixmap(size * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QRectF(1.0, 1.0, size.width() - 2.0, size.height() - 2.0));
    painter.setPen(Qt::white);
    QFont font("Microsoft YaHei UI", 9, QFont::DemiBold);
    font.setStyleStrategy(QFont::PreferAntialias);
    painter.setFont(font);
    painter.drawText(QRectF(0, 0, size.width(), size.height()), Qt::AlignCenter, name.left(1).toUpper());
    return pixmap;
}

QPixmap roundedPixmap(const QImage &image, const QSize &size)
{
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    path.addRoundedRect(pixmap.rect(), 14, 14);
    painter.setClipPath(path);
    painter.drawImage(pixmap.rect(), image);
    return pixmap;
}

QSize textLayoutSize(const QString &text, int maxBubbleWidth)
{
    QTextDocument document;
    QFont font("Microsoft YaHei UI", 10);
    font.setStyleStrategy(QFont::PreferAntialias);
    document.setDefaultFont(font);
    document.setDocumentMargin(0);
    document.setPlainText(text);
    document.adjustSize();

    const int idealTextWidth = qCeil(document.idealWidth());
    const int textWidth = qMax(1, qMin(maxBubbleWidth - 2 * kBubblePaddingH, idealTextWidth));
    document.setTextWidth(textWidth);
    const int textHeight = qCeil(document.size().height());
    return QSize(textWidth, textHeight);
}
}

MessageCell::MessageCell(QWidget *parent)
    : QWidget(parent)
    , _avatarLabel(new QLabel(this))
    , _bubbleWidget(new QWidget(this))
    , _textBrowser(new QTextBrowser(_bubbleWidget))
    , _imageLabel(new QLabel(_bubbleWidget))
{
    _avatarLabel->setFixedSize(kAvatarSize, kAvatarSize);

    _textBrowser->setFrameShape(QFrame::NoFrame);
    _textBrowser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _textBrowser->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _textBrowser->setReadOnly(true);
    _textBrowser->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    _textBrowser->setContentsMargins(0, 0, 0, 0);
    _textBrowser->document()->setDocumentMargin(0);
    _textBrowser->setStyleSheet("background:transparent; border:none; padding:0px; margin:0px;");

    _imageLabel->setScaledContents(true);
    _imageLabel->hide();
}

void MessageCell::setMessage(const MessageItem &message, int availableWidth)
{
    updateAvatar(message);

    const int maxBubbleWidth = qMin(kBubbleMaxWidth, qMax(80, availableWidth - 2 * (kSideMargin + kAvatarSize + 20)));
    int contentHeight = 0;
    int bubbleWidth = maxBubbleWidth;

    if (message.type == ChatMessageType::Text) {
        _textBrowser->show();
        _imageLabel->hide();
        _textBrowser->setPlainText(message.text);
        const QSize textSize = textLayoutSize(message.text, maxBubbleWidth);
        bubbleWidth = textSize.width() + 2 * kBubblePaddingH;
        contentHeight = textSize.height() + 2 * kBubblePaddingV;
        _bubbleWidget->setStyleSheet(message.outgoing
                                     ? "background:#d9f7be; border-radius:18px;"
                                     : "background:#ffffff; border-radius:18px;");
        _textBrowser->setGeometry(kBubblePaddingH, kBubblePaddingV, textSize.width(), textSize.height());
    } else {
        _textBrowser->hide();
        _imageLabel->show();
        const QSize sourceSize = message.image.isNull() ? QSize(200, 140) : message.image.size();
        const QSize bounded = sourceSize.scaled(240, 180, Qt::KeepAspectRatio);
        bubbleWidth = bounded.width();
        contentHeight = bounded.height();
        _imageLabel->setPixmap(roundedPixmap(message.image, bounded));
        _imageLabel->setGeometry(0, 0, bubbleWidth, contentHeight);
        _bubbleWidget->setStyleSheet("background:transparent;");
    }

    _bubbleWidget->resize(bubbleWidth, contentHeight);

    if (message.outgoing) {
        layoutOutgoing(bubbleWidth, contentHeight);
    } else {
        layoutIncoming(bubbleWidth, contentHeight);
    }

    setFixedHeight(qMax(contentHeight, kAvatarSize) + 18);
}

int MessageCell::heightForMessage(const MessageItem &message, int availableWidth)
{
    const int bubbleWidth = qMin(kBubbleMaxWidth, qMax(80, availableWidth - 2 * (kSideMargin + kAvatarSize + 20)));
    if (message.type == ChatMessageType::Text) {
        const QSize textSize = textLayoutSize(message.text, bubbleWidth);
        return qMax(textSize.height() + 2 * kBubblePaddingV, kAvatarSize) + 18;
    }

    const QSize sourceSize = message.image.isNull() ? QSize(200, 140) : message.image.size();
    return qMax(sourceSize.scaled(240, 180, Qt::KeepAspectRatio).height(), kAvatarSize) + 18;
}

void MessageCell::updateAvatar(const MessageItem &message)
{
    _avatarLabel->setPixmap(buildAvatarPixmap(message.senderName, message.avatarColor, _avatarLabel->size()));
}

void MessageCell::layoutOutgoing(int bubbleWidth, int contentHeight)
{
    const int y = 8;
    const int avatarX = width() - kSideMargin - kAvatarSize;
    const int bubbleX = avatarX - 12 - bubbleWidth;
    _avatarLabel->move(avatarX, y);
    _bubbleWidget->setGeometry(bubbleX, y, bubbleWidth, contentHeight);
}

void MessageCell::layoutIncoming(int bubbleWidth, int contentHeight)
{
    const int y = 8;
    const int avatarX = kSideMargin;
    const int bubbleX = avatarX + kAvatarSize + 12;
    _avatarLabel->move(avatarX, y);
    _bubbleWidget->setGeometry(bubbleX, y, bubbleWidth, contentHeight);
}
