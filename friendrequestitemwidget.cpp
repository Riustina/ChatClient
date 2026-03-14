#include "friendrequestitemwidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
QPixmap buildAvatarPixmap(const QString &name, const QColor &color, const QSize &size)
{
    const qreal dpr = 2.0;
    QPixmap pixmap(size * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QRectF(1.0, 1.0, size.width() - 2.0, size.height() - 2.0));

    painter.setPen(Qt::white);
    QFont font("Microsoft YaHei UI", 10, QFont::DemiBold);
    font.setStyleStrategy(QFont::PreferAntialias);
    painter.setFont(font);
    painter.drawText(QRectF(0, 0, size.width(), size.height()), Qt::AlignCenter, name.left(1));
    return pixmap;
}
}

FriendRequestItemWidget::FriendRequestItemWidget(QWidget *parent)
    : QWidget(parent)
    , _avatarLabel(new QLabel(this))
    , _nameLabel(new QLabel(this))
    , _detailLabel(new QLabel(this))
    , _statusLabel(new QLabel(this))
    , _acceptButton(new QPushButton(QStringLiteral("同意"), this))
    , _rejectButton(new QPushButton(QStringLiteral("拒绝"), this))
{
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(82);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(14, 12, 14, 12);
    rootLayout->setSpacing(12);

    _avatarLabel->setFixedSize(42, 42);

    auto *textLayout = new QVBoxLayout;
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(3);
    textLayout->addWidget(_nameLabel);
    textLayout->addWidget(_detailLabel);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setContentsMargins(0, 0, 0, 0);
    buttonRow->setSpacing(8);
    buttonRow->addWidget(_rejectButton);
    buttonRow->addWidget(_acceptButton);

    auto *rightLayout = new QVBoxLayout;
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(6);
    rightLayout->addWidget(_statusLabel, 0, Qt::AlignRight);
    rightLayout->addLayout(buttonRow);
    rightLayout->addStretch();

    rootLayout->addWidget(_avatarLabel, 0, Qt::AlignTop);
    rootLayout->addLayout(textLayout, 1);
    rootLayout->addLayout(rightLayout);

    connect(_acceptButton, &QPushButton::clicked, this, [this]() {
        emit acceptClicked(_requestId);
    });
    connect(_rejectButton, &QPushButton::clicked, this, [this]() {
        emit rejectClicked(_requestId);
    });
}

void FriendRequestItemWidget::setRequestItem(const FriendRequestItem &item)
{
    _requestId = item.id;
    _nameLabel->setText(item.name);
    _detailLabel->setText(QStringLiteral("备注：%1").arg(item.remark.trimmed().isEmpty() ? QStringLiteral("无") : item.remark.trimmed()));

    const bool pendingIncoming = item.direction == FriendRequestDirection::Incoming && item.state == FriendRequestState::Pending;
    _statusLabel->setVisible(item.direction == FriendRequestDirection::Outgoing || item.state != FriendRequestState::Pending);
    _acceptButton->setVisible(pendingIncoming);
    _rejectButton->setVisible(pendingIncoming);

    if (item.state == FriendRequestState::Pending) {
        _statusLabel->setText(QStringLiteral("待验证"));
    } else if (item.state == FriendRequestState::Rejected) {
        _statusLabel->setText(QStringLiteral("已拒绝"));
    } else {
        _statusLabel->setText(QStringLiteral("已添加"));
    }

    updateAvatar(item);
    updateStyles(item);
}

void FriendRequestItemWidget::updateAvatar(const FriendRequestItem &item)
{
    _avatarLabel->setPixmap(buildAvatarPixmap(item.name, item.avatarColor, _avatarLabel->size()));
}

void FriendRequestItemWidget::updateStyles(const FriendRequestItem &item)
{
    setStyleSheet(
        "FriendRequestItemWidget { background:#FCF8FF; border:1px solid #ebe4f1; border-radius:16px; }"
        "QLabel { background:transparent; }"
        "QLabel { font: 9pt 'Microsoft YaHei UI'; color:#475569; }"
        "QPushButton { min-width:68px; min-height:30px; border:none; border-radius:15px; font: 9pt 'Microsoft YaHei UI'; }"
        "QPushButton:pressed { padding-top:1px; }");

    QFont titleFont("Microsoft YaHei UI", 10, QFont::DemiBold);
    titleFont.setStyleStrategy(QFont::PreferAntialias);
    _nameLabel->setFont(titleFont);
    _nameLabel->setStyleSheet("color:#111827;");
    _detailLabel->setStyleSheet("color:#64748b;");
    _detailLabel->setWordWrap(true);

    QString statusStyle;
    if (item.state == FriendRequestState::Pending) {
        statusStyle = "background:#efe9f7; color:#6b4fa0; border-radius:10px; padding:4px 10px;";
    } else if (item.state == FriendRequestState::Rejected) {
        statusStyle = "background:#f3e8e8; color:#9a4545; border-radius:10px; padding:4px 10px;";
    } else {
        statusStyle = "background:#e5ece7; color:#2f6b41; border-radius:10px; padding:4px 10px;";
    }
    _statusLabel->setStyleSheet(statusStyle);
    _acceptButton->setStyleSheet("background:#CBCACF; color:#111827;");
    _rejectButton->setStyleSheet("background:#EAE9EF; color:#334155;");
}
