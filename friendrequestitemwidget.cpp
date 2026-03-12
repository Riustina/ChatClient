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

    auto *rightLayout = new QVBoxLayout;
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(6);
    rightLayout->addWidget(_statusLabel, 0, Qt::AlignRight);
    rightLayout->addWidget(_acceptButton, 0, Qt::AlignRight);
    rightLayout->addStretch();

    rootLayout->addWidget(_avatarLabel, 0, Qt::AlignTop);
    rootLayout->addLayout(textLayout, 1);
    rootLayout->addLayout(rightLayout);

    connect(_acceptButton, &QPushButton::clicked, this, [this]() {
        emit acceptClicked(_requestId);
    });
}

void FriendRequestItemWidget::setRequestItem(const FriendRequestItem &item)
{
    _requestId = item.id;
    _nameLabel->setText(item.name);

    if (item.direction == FriendRequestDirection::Outgoing) {
        _detailLabel->setText(QStringLiteral("你已向对方发送好友申请"));
    } else if (item.state == FriendRequestState::Pending) {
        _detailLabel->setText(QStringLiteral("对方向你发送了好友申请"));
    } else {
        _detailLabel->setText(QStringLiteral("你们已经是好友"));
    }

    _statusLabel->setVisible(item.direction == FriendRequestDirection::Outgoing || item.state == FriendRequestState::Added);
    _acceptButton->setVisible(item.direction == FriendRequestDirection::Incoming && item.state == FriendRequestState::Pending);
    _statusLabel->setText(item.state == FriendRequestState::Pending
                          ? QStringLiteral("待验证")
                          : QStringLiteral("已添加"));

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

    const QString statusStyle = item.state == FriendRequestState::Pending
        ? "background:#efe9f7; color:#6b4fa0; border-radius:10px; padding:4px 10px;"
        : "background:#e5ece7; color:#2f6b41; border-radius:10px; padding:4px 10px;";
    _statusLabel->setStyleSheet(statusStyle);
    _acceptButton->setStyleSheet("background:#CBCACF; color:#111827;");
}
