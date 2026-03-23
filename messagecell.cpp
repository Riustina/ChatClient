#include "messagecell.h"

#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QFileDialog>
#include <QFrame>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QGraphicsDropShadowEffect>
#include <QDialog>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScreen>
#include <QScrollBar>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextCharFormat>
#include <QPointer>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtMath>
#include <QtConcurrent/QtConcurrent>

namespace {
constexpr int kAvatarSize     = 34;
constexpr int kSideMargin     = 8;
constexpr int kBubblePaddingH = 14;
constexpr int kBubblePaddingV = 8;
constexpr int kBubbleMaxWidth = 2250;
constexpr int kImagePreviewMaxWidth = 360;
constexpr int kImagePreviewMaxHeight = 300;

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

QColor fallbackAvatarColor(const QString &name)
{
    static const QList<QColor> colors = {
        QColor("#4f46e5"),
        QColor("#0f766e"),
        QColor("#ea580c"),
        QColor("#dc2626"),
        QColor("#2563eb"),
        QColor("#7c3aed")
    };

    int seed = 0;
    for (const QChar ch : name) {
        seed += ch.unicode();
    }
    if (seed < 0) {
        seed = -seed;
    }
    return colors[seed % colors.size()];
}

QPixmap roundedPixmap(const QImage &image, const QSize &size)
{
    const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
    QPixmap pixmap(size * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, size.width(), size.height()), 14, 14);
    painter.setClipPath(path);
    if (!image.isNull()) {
        const QImage scaled = image.scaled(size * dpr, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        const QSizeF drawSize = QSizeF(scaled.width() / dpr, scaled.height() / dpr);
        const QPointF topLeft((size.width() - drawSize.width()) / 2.0,
                              (size.height() - drawSize.height()) / 2.0);
        painter.drawImage(QRectF(topLeft, drawSize), scaled);
    }
    return pixmap;
}

QSize textLayoutSize(const QString &text, int maxBubbleWidth)
{
    QFont font("Microsoft YaHei UI", 10);
    font.setStyleStrategy(QFont::PreferAntialias);
    const QFontMetrics metrics(font);
    const int maxTextWidth    = qMax(1, maxBubbleWidth - 2 * kBubblePaddingH);
    const int singleLineWidth = qMax(metrics.horizontalAdvance(text),
                                     metrics.boundingRect(text).width());
    // 留出一点额外空间，避免不同机器上的字形渲染把最后一个字符裁掉。
    const int textWidth       = qMin(maxTextWidth, qMax(1, singleLineWidth + 6));
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

class ImagePreviewDialog final : public QDialog {
public:
    explicit ImagePreviewDialog(const QImage &image, QWidget *parent = nullptr)
        : QDialog(parent)
        , _sourceImage(image)
        , _scrollArea(new QScrollArea(this))
        , _imageLabel(new QLabel(_scrollArea))
        , _scaleFactor(1.0)
        , _fitToWindow(true)
    {
        setAttribute(Qt::WA_DeleteOnClose, true);
        setWindowTitle(QStringLiteral("图片查看"));
        setModal(false);
        resize(1100, 800);
        setMinimumSize(720, 520);
        setStyleSheet(
            "QDialog { background:#101826; }"
            "QFrame { background:#101826; }"
            "QLabel { background:transparent; color:white; }"
            "QPushButton {"
            "  background:#f8fafc;"
            "  border:none;"
            "  border-radius:8px;"
            "  padding:7px 14px;"
            "  font: 9pt 'Microsoft YaHei UI';"
            "  color:#111827;"
            "}"
            "QPushButton:hover { background:#e2e8f0; }"
            "QPushButton:pressed { background:#cbd5e1; }");

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(18, 18, 18, 18);
        layout->setSpacing(12);

        auto *toolbar = new QFrame(this);
        auto *toolbarLayout = new QHBoxLayout(toolbar);
        toolbarLayout->setContentsMargins(0, 0, 0, 0);
        toolbarLayout->setSpacing(8);

        auto *fitButton = new QPushButton(QStringLiteral("适应窗口"), toolbar);
        auto *actualButton = new QPushButton(QStringLiteral("100%"), toolbar);
        auto *zoomOutButton = new QPushButton(QStringLiteral("-"), toolbar);
        auto *zoomInButton  = new QPushButton(QStringLiteral("+"), toolbar);
        auto *saveButton   = new QPushButton(QStringLiteral("保存图片"), toolbar);
        _zoomLabel = new QLabel(toolbar);
        _zoomLabel->setMinimumWidth(56);

        toolbarLayout->addWidget(fitButton);
        toolbarLayout->addWidget(actualButton);
        toolbarLayout->addWidget(zoomOutButton);
        toolbarLayout->addWidget(zoomInButton);
        toolbarLayout->addWidget(_zoomLabel);
        toolbarLayout->addStretch(1);
        toolbarLayout->addWidget(saveButton);
        layout->addWidget(toolbar);

        _scrollArea->setFrameShape(QFrame::NoFrame);
        _scrollArea->setAlignment(Qt::AlignCenter);
        _scrollArea->setWidgetResizable(false);
        _scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        _scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        _scrollArea->setStyleSheet(
            "QScrollArea { background:transparent; }"
            "QScrollBar:horizontal, QScrollBar:vertical { background:rgba(255,255,255,0.08); border:none; }"
            "QScrollBar:horizontal { height:10px; margin:0px 18px 0px 18px; }"
            "QScrollBar:vertical { width:10px; margin:18px 0px 18px 0px; }"
            "QScrollBar::handle:horizontal, QScrollBar::handle:vertical { background:rgba(255,255,255,0.38); border-radius:5px; }"
            "QScrollBar::handle:horizontal:hover, QScrollBar::handle:vertical:hover { background:rgba(255,255,255,0.52); }"
            "QScrollBar::add-line, QScrollBar::sub-line, QScrollBar::add-page, QScrollBar::sub-page { background:transparent; border:none; }");
        layout->addWidget(_scrollArea, 1);

        _imageLabel->setAlignment(Qt::AlignCenter);
        _imageLabel->setStyleSheet("background:transparent;");
        _imageLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        _imageLabel->setScaledContents(false);
        _scrollArea->setWidget(_imageLabel);
        _imageLabel->installEventFilter(this);
        _scrollArea->viewport()->installEventFilter(this);

        connect(fitButton, &QPushButton::clicked, this, [this]() {
            _fitToWindow = true;
            updateImageDisplay();
        });
        connect(actualButton, &QPushButton::clicked, this, [this]() {
            _fitToWindow = false;
            _scaleFactor = 1.0;
            updateImageDisplay();
        });
        connect(zoomOutButton, &QPushButton::clicked, this, [this]() {
            _fitToWindow = false;
            _scaleFactor = qMax(0.1, _scaleFactor / 1.2);
            updateImageDisplay();
        });
        connect(zoomInButton, &QPushButton::clicked, this, [this]() {
            _fitToWindow = false;
            _scaleFactor = qMin(8.0, _scaleFactor * 1.2);
            updateImageDisplay();
        });
        connect(saveButton, &QPushButton::clicked, this, [this, saveButton]() {
            const QString path = QFileDialog::getSaveFileName(
                this,
                QStringLiteral("保存图片"),
                QDir::homePath() + QStringLiteral("/image.png"),
                QStringLiteral("图片文件 (*.png *.jpg *.bmp);;PNG (*.png);;JPEG (*.jpg);;BMP (*.bmp)")
            );
            if (path.isEmpty()) {
                return;
            }

            QString format = QStringLiteral("PNG");
            if (path.endsWith(QStringLiteral(".jpg"), Qt::CaseInsensitive)
                || path.endsWith(QStringLiteral(".jpeg"), Qt::CaseInsensitive)) {
                format = QStringLiteral("JPEG");
            } else if (path.endsWith(QStringLiteral(".bmp"), Qt::CaseInsensitive)) {
                format = QStringLiteral("BMP");
            }

            // 保存期间禁用按钮，防止重复点击
            saveButton->setEnabled(false);
            saveButton->setText(QStringLiteral("保存中…"));

            const QImage imageCopy = _sourceImage;
            const QByteArray fmt = format.toLatin1();
            QPointer<ImagePreviewDialog> self = this;
            QtConcurrent::run([imageCopy, path, fmt, self, saveButton]() {
                const bool ok = imageCopy.save(path, fmt.constData());
                // 回到主线程更新 UI
                QMetaObject::invokeMethod(qApp, [ok, self, saveButton]() {
                    if (!self) {
                        return;
                    }
                    saveButton->setEnabled(true);
                    saveButton->setText(QStringLiteral("保存图片"));
                    if (!ok) {
                        QMessageBox::warning(self,
                                             QStringLiteral("保存失败"),
                                             QStringLiteral("图片保存失败，请检查路径或权限。"));
                    }
                }, Qt::QueuedConnection);
            });
        });

        QTimer::singleShot(0, this, [this]() { updateImageDisplay(); });
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QDialog::resizeEvent(event);
        if (_fitToWindow) {
            updateImageDisplay();
        }
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Escape) {
            close();
            return;
        }
        if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) {
            _fitToWindow = false;
            _scaleFactor = qMin(8.0, _scaleFactor * 1.2);
            updateImageDisplay();
            return;
        }
        if (event->key() == Qt::Key_Minus || event->key() == Qt::Key_Underscore) {
            _fitToWindow = false;
            _scaleFactor = qMax(0.1, _scaleFactor / 1.2);
            updateImageDisplay();
            return;
        }
        if (event->key() == Qt::Key_0) {
            _fitToWindow = false;
            _scaleFactor = 1.0;
            updateImageDisplay();
            return;
        }
        QDialog::keyPressEvent(event);
    }

    void wheelEvent(QWheelEvent *event) override
    {
        if (event->angleDelta().y() != 0) {
            _fitToWindow = false;
            if (event->angleDelta().y() > 0) {
                _scaleFactor = qMin(8.0, _scaleFactor * 1.15);
            } else {
                _scaleFactor = qMax(0.1, _scaleFactor / 1.15);
            }
            updateImageDisplay();
            event->accept();
            return;
        }
        QDialog::wheelEvent(event);
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == _imageLabel || watched == _scrollArea->viewport()) {
            if (event->type() == QEvent::MouseButtonPress) {
                auto *mouseEvent = static_cast<QMouseEvent *>(event);
                if (mouseEvent->button() == Qt::LeftButton) {
                    _dragging = true;
                    _lastDragPos = mouseEvent->globalPosition().toPoint();
                    _imageLabel->setCursor(Qt::ClosedHandCursor);
                    return true;
                }
            } else if (event->type() == QEvent::MouseMove && _dragging) {
                auto *mouseEvent = static_cast<QMouseEvent *>(event);
                const QPoint currentPos = mouseEvent->globalPosition().toPoint();
                const QPoint delta = currentPos - _lastDragPos;
                _lastDragPos = currentPos;
                _scrollArea->horizontalScrollBar()->setValue(_scrollArea->horizontalScrollBar()->value() - delta.x());
                _scrollArea->verticalScrollBar()->setValue(_scrollArea->verticalScrollBar()->value() - delta.y());
                return true;
            } else if (event->type() == QEvent::MouseButtonRelease && _dragging) {
                auto *mouseEvent = static_cast<QMouseEvent *>(event);
                if (mouseEvent->button() == Qt::LeftButton) {
                    _dragging = false;
                    _imageLabel->setCursor(Qt::OpenHandCursor);
                    return true;
                }
            }
        }
        return QDialog::eventFilter(watched, event);
    }

private:
    void updateImageDisplay()
    {
        if (_sourceImage.isNull()) {
            _imageLabel->clear();
            _zoomLabel->setText(QStringLiteral("0%"));
            return;
        }

        QSize targetSize = _sourceImage.size();
        if (_fitToWindow) {
            const QSize available = _scrollArea->viewport()->size() - QSize(24, 24);
            if (available.width() <= 0 || available.height() <= 0) {
                _zoomLabel->setText(QStringLiteral("适应"));
                return;
            }
            targetSize = _sourceImage.size().scaled(available.expandedTo(QSize(1, 1)),
                                                    Qt::KeepAspectRatio);
            _scaleFactor = qreal(targetSize.width()) / qMax(1, _sourceImage.width());
        } else {
            targetSize = QSize(qRound(_sourceImage.width() * _scaleFactor),
                               qRound(_sourceImage.height() * _scaleFactor));
        }

        const qreal dpr = devicePixelRatioF();
        const QImage scaled = _sourceImage.scaled(targetSize * dpr,
                                                  Qt::KeepAspectRatio,
                                                  Qt::SmoothTransformation);
        QPixmap pixmap = QPixmap::fromImage(scaled);
        pixmap.setDevicePixelRatio(dpr);
        _imageLabel->setPixmap(pixmap);
        _imageLabel->resize(targetSize);
        _imageLabel->setCursor(_fitToWindow ? Qt::ArrowCursor : Qt::OpenHandCursor);
        _zoomLabel->setText(QStringLiteral("%1%").arg(qRound(_scaleFactor * 100.0)));
    }

    QImage _sourceImage;
    QScrollArea *_scrollArea;
    QLabel *_imageLabel;
    QLabel *_zoomLabel;
    qreal _scaleFactor;
    bool _fitToWindow;
    bool _dragging = false;
    QPoint _lastDragPos;
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
    , _statusButton(new QPushButton(this))
{
    _avatarLabel->setFixedSize(kAvatarSize, kAvatarSize);
    _bubbleWidget->setAttribute(Qt::WA_StyledBackground, true);

    _textBrowser->setFrameShape(QFrame::NoFrame);
    _textBrowser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _textBrowser->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _textBrowser->setReadOnly(true);
    _textBrowser->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    _textBrowser->setContentsMargins(0, 0, 0, 0);
    _textBrowser->document()->setDocumentMargin(0);
    _textBrowser->setStyleSheet("background:transparent; border:none; padding:0px; margin:0px;");
    _textBrowser->setContextMenuPolicy(Qt::CustomContextMenu);
    _textBrowser->viewport()->setAttribute(Qt::WA_StyledBackground, true);
    _textBrowser->viewport()->setStyleSheet("background:transparent;");
    _textBrowser->viewport()->setCursor(Qt::IBeamCursor);

    // 挂上 viewport 过滤器，随 _textBrowser 自动销毁
    new BrowserViewportFilter(_textBrowser);

    connect(_textBrowser, &QTextBrowser::customContextMenuRequested,
            this, &MessageCell::showTextContextMenu);

    _imageLabel->setScaledContents(false);
    _imageLabel->setAttribute(Qt::WA_StyledBackground, true);
    _imageLabel->setStyleSheet("background:transparent;");
    _imageLabel->installEventFilter(this);
    _imageLabel->hide();

    _statusButton->hide();
    _statusButton->setFlat(true);
    _statusButton->setCursor(Qt::PointingHandCursor);
    connect(_statusButton, &QPushButton::clicked, this, [this]() {
        if (!_currentClientMsgId.isEmpty()) {
            emit retryRequested(_currentClientMsgId);
        }
    });
}

MessageCell::~MessageCell() = default;

bool MessageCell::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == _imageLabel && event->type() == QEvent::MouseButtonDblClick) {
        if (!_currentImage.isNull()) {
            showImagePreview();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void MessageCell::setMessage(const MessageItem &message, int availableWidth)
{
    updateAvatar(message);
    _currentImage = message.image;
    _currentClientMsgId = message.clientMsgId;

    const int maxBubbleWidth = qMin(kBubbleMaxWidth,
                                    qMax(80, availableWidth - 2 * (kSideMargin + kAvatarSize + 20)));
    int contentHeight = 0;
    int bubbleWidth   = maxBubbleWidth;

    if (message.type == ChatMessageType::Text) {
        _textBrowser->show();
        _imageLabel->hide();
        _textBrowser->setPlainText(message.text);
        const QColor textColor = message.outgoing ? QColor("#111827") : QColor("#1f2937");
        _textBrowser->setStyleSheet(QStringLiteral("background:transparent; border:none; padding:0px; margin:0px; color:%1;").arg(textColor.name()));
        QTextCursor cursor(_textBrowser->document());
        cursor.select(QTextCursor::Document);
        QTextCharFormat format;
        format.setForeground(textColor);
        cursor.mergeCharFormat(format);
        _textBrowser->setTextCursor(cursor);
        _textBrowser->moveCursor(QTextCursor::Start);
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
        const QSize bounded    = sourceSize.scaled(kImagePreviewMaxWidth, kImagePreviewMaxHeight, Qt::KeepAspectRatio);
        bubbleWidth   = bounded.width();
        contentHeight = bounded.height();
        _imageLabel->setPixmap(roundedPixmap(message.image, bounded));
        _imageLabel->setGeometry(0, 0, bubbleWidth, contentHeight);
        _bubbleWidget->setStyleSheet("background:transparent;");
    }

    _bubbleWidget->resize(bubbleWidth, contentHeight);

    int bubbleX = 0;
    if (message.outgoing)
    {
        bubbleX = width() - kSideMargin - kAvatarSize - 12 - bubbleWidth;
        layoutOutgoing(bubbleWidth, contentHeight);
    }
    else
    {
        bubbleX = kSideMargin + kAvatarSize + 12;
        layoutIncoming(bubbleWidth, contentHeight);
    }

    updateStatusWidget(message, bubbleX, bubbleWidth, contentHeight);

    const int extraHeight = (message.outgoing && message.sendState != MessageSendState::Sent) ? 18 : 0;
    // setFixedHeight(qMax(contentHeight + extraHeight, kAvatarSize) + 18);
}

int MessageCell::heightForMessage(const MessageItem &message, int availableWidth)
{
    const int bubbleWidth = qMin(kBubbleMaxWidth,
                                 qMax(80, availableWidth - 2 * (kSideMargin + kAvatarSize + 20)));
    if (message.type == ChatMessageType::Text) {
        const QSize textSize = textLayoutSize(message.text, bubbleWidth);
        const int extraHeight = (message.outgoing && message.sendState != MessageSendState::Sent) ? 18 : 0;
        return qMax(textSize.height() + 2 * kBubblePaddingV + extraHeight, kAvatarSize) + 18;
    }
    const QSize sourceSize = message.image.isNull() ? QSize(200, 140) : message.image.size();
    const int extraHeight = (message.outgoing && message.sendState != MessageSendState::Sent) ? 18 : 0;
    return qMax(sourceSize.scaled(kImagePreviewMaxWidth, kImagePreviewMaxHeight, Qt::KeepAspectRatio).height() + extraHeight, kAvatarSize) + 18;
}

void MessageCell::updateAvatar(const MessageItem &message)
{
    const QString displayName = message.senderName.trimmed().isEmpty()
        ? (message.outgoing ? QStringLiteral("我") : QStringLiteral("?"))
        : message.senderName.trimmed();
    const QColor avatarColor = message.avatarColor.isValid()
        ? message.avatarColor
        : fallbackAvatarColor(displayName);
    _avatarLabel->setPixmap(
        buildAvatarPixmap(displayName, avatarColor, _avatarLabel->size()));
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

void MessageCell::showImagePreview() const
{
    if (_currentImage.isNull()) {
        return;
    }

    auto *dialog = new ImagePreviewDialog(_currentImage, window());
    dialog->show();
}

void MessageCell::updateStatusWidget(const MessageItem &message, int bubbleX, int bubbleWidth, int contentHeight)
{
    if (!message.outgoing || message.sendState == MessageSendState::Sent) {
        _statusButton->hide();
        return;
    }

    const int y = 8 + contentHeight + 2;
    if (message.sendState == MessageSendState::Sending) {
        _statusButton->setText(QStringLiteral("发送中"));
        _statusButton->setEnabled(false);
        _statusButton->setCursor(Qt::ArrowCursor);
        _statusButton->setStyleSheet(
            "QPushButton { background:transparent; border:none; padding:0px; font: 8pt 'Microsoft YaHei UI'; color:#94a3b8; }");
    } else {
        _statusButton->setText(QStringLiteral("发送失败，点击重试"));
        _statusButton->setEnabled(true);
        _statusButton->setCursor(Qt::PointingHandCursor);
        _statusButton->setStyleSheet(
            "QPushButton { background:transparent; border:none; padding:0px; font: 8pt 'Microsoft YaHei UI'; color:#ef4444; }"
            "QPushButton:hover { color:#dc2626; text-decoration:underline; }");
    }
    _statusButton->adjustSize();
    _statusButton->move(bubbleX + bubbleWidth - _statusButton->width(), y);
    _statusButton->show();
    _statusButton->raise();
}
