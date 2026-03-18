#include "messagecell.h"

#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QFrame>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScreen>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>
#include <QtMath>

namespace {
constexpr int kAvatarSize     = 34;
constexpr int kSideMargin     = 8;
constexpr int kBubblePaddingH = 14;
constexpr int kBubblePaddingV = 8;
constexpr int kBubbleMaxWidth = 2250;

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
    QFont font("Microsoft YaHei UI", 10);
    font.setStyleStrategy(QFont::PreferAntialias);
    const QFontMetrics metrics(font);
    const int maxTextWidth    = qMax(1, maxBubbleWidth - 2 * kBubblePaddingH);
    const int singleLineWidth = qMax(1, metrics.horizontalAdvance(text));
    const int textWidth       = qMin(maxTextWidth, singleLineWidth);
    const QRect wrappedRect   = metrics.boundingRect(QRect(0, 0, textWidth, 100000),
                                                   Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
                                                   text);
    return QSize(textWidth, qMax(metrics.height(), wrappedRect.height()));
}

// ---------------------------------------------------------------------------
// 右键弹出菜单
// 构造时立即快照文本，与之后 selection 状态完全解耦。
// ---------------------------------------------------------------------------
class TextActionPopup final : public QWidget {
public:
    explicit TextActionPopup(QTextBrowser *browser, const QPoint &globalPos)
        : QWidget(nullptr, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
    {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_DeleteOnClose, true);

        // 立即快照：有选中就复制选中，否则复制全文
        const QString selected   = browser->textCursor().selectedText();
        const QString textToCopy = selected.isEmpty() ? browser->toPlainText() : selected;

        auto *outerLayout = new QVBoxLayout(this);
        outerLayout->setContentsMargins(18, 18, 18, 18);

        auto *panel = new QFrame(this);
        panel->setStyleSheet(
            "QFrame {"
            "  background:#fcfcfd;"
            "  border:1px solid rgba(226,232,240,0.9);"
            "  border-radius:14px;"
            "}"
            "QPushButton {"
            "  background:transparent;"
            "  border:none;"
            "  border-radius:10px;"
            "  padding:9px 20px;"
            "  text-align:left;"
            "  font: 9pt 'Microsoft YaHei UI';"
            "  color:#111827;"
            "}"
            "QPushButton:hover { background:#eef2ff; }");

        auto *shadow = new QGraphicsDropShadowEffect(panel);
        shadow->setBlurRadius(36);
        shadow->setOffset(0, 8);
        shadow->setColor(QColor(15, 23, 42, 70));
        panel->setGraphicsEffect(shadow);

        auto *panelLayout = new QVBoxLayout(panel);
        panelLayout->setContentsMargins(8, 8, 8, 8);
        panelLayout->setSpacing(4);

        auto *copyButton = new QPushButton(QStringLiteral("Copy"), panel);
        panelLayout->addWidget(copyButton);
        outerLayout->addWidget(panel);

        connect(copyButton, &QPushButton::clicked, this, [textToCopy, this]() {
            if (!textToCopy.isEmpty())
                qApp->clipboard()->setText(textToCopy);
            close();
        });

        adjustSize();

        // 定位，确保不超出屏幕
        QPoint pos = globalPos + QPoint(10, 14);
        if (QScreen *s = qApp->screenAt(globalPos)) {
            const QRect avail = s->availableGeometry();
            if (pos.x() + width()  > avail.right()  - 8) pos.setX(avail.right()  - width()  - 8);
            if (pos.y() + height() > avail.bottom() - 8) pos.setY(avail.bottom() - height() - 8);
            pos.setX(qMax(avail.left() + 8, pos.x()));
            pos.setY(qMax(avail.top()  + 8, pos.y()));
        }
        move(pos);
    }
};

// ---------------------------------------------------------------------------
// 每个 QTextBrowser 挂一个此过滤器（parent 设为 browser，随之自动销毁）。
// 职责：
//   1. 监听 viewport 自身的 CursorChange：任何人改光标时强制改回 IBeam
//   2. 监听 qApp 的 MouseButtonPress：点击 viewport 外时清除 selection
// ---------------------------------------------------------------------------
class BrowserViewportFilter final : public QObject {
public:
    explicit BrowserViewportFilter(QTextBrowser *browser)
        : QObject(browser)
        , _browser(browser)
    {
        browser->viewport()->installEventFilter(this);
        qApp->installEventFilter(this);
    }

    ~BrowserViewportFilter() override
    {
        qApp->removeEventFilter(this);
        // viewport 上的 filter 随 viewport 销毁自动清理，无需手动 remove
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        // --- viewport 上：任何 CursorChange 都强制改回 IBeam ---
        if (watched == _browser->viewport() && event->type() == QEvent::CursorChange) {
            if (_browser->viewport()->cursor().shape() != Qt::IBeamCursor)
                _browser->viewport()->setCursor(Qt::IBeamCursor);
            return false;
        }

        // --- qApp 上：鼠标按下时若点击在 viewport 外则清除 selection ---
        if (watched != _browser->viewport() && event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(event);
            const QPoint gp = me->globalPosition().toPoint();
            auto *vp = _browser->viewport();
            const QRect vpRect(vp->mapToGlobal(QPoint(0, 0)), vp->size());
            if (!vpRect.contains(gp)) {
                QTextCursor cur = _browser->textCursor();
                if (cur.hasSelection()) {
                    cur.clearSelection();
                    _browser->setTextCursor(cur);
                }
            }
        }

        return false;
    }

private:
    QTextBrowser *_browser;
};

} // namespace

// ===========================================================================
// MessageCell
// ===========================================================================

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
    _textBrowser->setContextMenuPolicy(Qt::CustomContextMenu);
    _textBrowser->viewport()->setCursor(Qt::IBeamCursor);

    // 挂上 viewport 过滤器，随 _textBrowser 自动销毁
    new BrowserViewportFilter(_textBrowser);

    connect(_textBrowser, &QTextBrowser::customContextMenuRequested,
            this, &MessageCell::showTextContextMenu);

    _imageLabel->setScaledContents(true);
    _imageLabel->hide();
}

MessageCell::~MessageCell() = default;

void MessageCell::setMessage(const MessageItem &message, int availableWidth)
{
    updateAvatar(message);

    const int maxBubbleWidth = qMin(kBubbleMaxWidth,
                                    qMax(80, availableWidth - 2 * (kSideMargin + kAvatarSize + 20)));
    int contentHeight = 0;
    int bubbleWidth   = maxBubbleWidth;

    if (message.type == ChatMessageType::Text) {
        _textBrowser->show();
        _imageLabel->hide();
        _textBrowser->setPlainText(message.text);
        const QSize textSize = textLayoutSize(message.text, maxBubbleWidth);
        bubbleWidth   = textSize.width() + 2 * kBubblePaddingH;
        contentHeight = textSize.height() + 2 * kBubblePaddingV;
        _bubbleWidget->setStyleSheet(message.outgoing
                                         ? "background:#d9f7be; border-radius:18px;"
                                         : "background:#ffffff; border-radius:18px;");
        _textBrowser->setGeometry(kBubblePaddingH, kBubblePaddingV,
                                  textSize.width(), textSize.height());
    } else {
        _textBrowser->hide();
        _imageLabel->show();
        const QSize sourceSize = message.image.isNull() ? QSize(200, 140) : message.image.size();
        const QSize bounded    = sourceSize.scaled(240, 180, Qt::KeepAspectRatio);
        bubbleWidth   = bounded.width();
        contentHeight = bounded.height();
        _imageLabel->setPixmap(roundedPixmap(message.image, bounded));
        _imageLabel->setGeometry(0, 0, bubbleWidth, contentHeight);
        _bubbleWidget->setStyleSheet("background:transparent;");
    }

    _bubbleWidget->resize(bubbleWidth, contentHeight);

    if (message.outgoing)
        layoutOutgoing(bubbleWidth, contentHeight);
    else
        layoutIncoming(bubbleWidth, contentHeight);

    setFixedHeight(qMax(contentHeight, kAvatarSize) + 18);
}

int MessageCell::heightForMessage(const MessageItem &message, int availableWidth)
{
    const int bubbleWidth = qMin(kBubbleMaxWidth,
                                 qMax(80, availableWidth - 2 * (kSideMargin + kAvatarSize + 20)));
    if (message.type == ChatMessageType::Text) {
        const QSize textSize = textLayoutSize(message.text, bubbleWidth);
        return qMax(textSize.height() + 2 * kBubblePaddingV, kAvatarSize) + 18;
    }
    const QSize sourceSize = message.image.isNull() ? QSize(200, 140) : message.image.size();
    return qMax(sourceSize.scaled(240, 180, Qt::KeepAspectRatio).height(), kAvatarSize) + 18;
}

void MessageCell::updateAvatar(const MessageItem &message)
{
    _avatarLabel->setPixmap(
        buildAvatarPixmap(message.senderName, message.avatarColor, _avatarLabel->size()));
}

void MessageCell::layoutOutgoing(int bubbleWidth, int contentHeight)
{
    const int y       = 8;
    const int avatarX = width() - kSideMargin - kAvatarSize;
    const int bubbleX = avatarX - 12 - bubbleWidth;
    _avatarLabel->move(avatarX, y);
    _bubbleWidget->setGeometry(bubbleX, y, bubbleWidth, contentHeight);
}

void MessageCell::layoutIncoming(int bubbleWidth, int contentHeight)
{
    const int y       = 8;
    const int avatarX = kSideMargin;
    const int bubbleX = avatarX + kAvatarSize + 12;
    _avatarLabel->move(avatarX, y);
    _bubbleWidget->setGeometry(bubbleX, y, bubbleWidth, contentHeight);
}

void MessageCell::showTextContextMenu(const QPoint &pos)
{
    const QPoint globalPos = _textBrowser->viewport()->mapToGlobal(pos);
    auto *popup = new TextActionPopup(_textBrowser, globalPos);
    popup->show();
}