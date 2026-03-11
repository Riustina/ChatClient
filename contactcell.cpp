#include "contactcell.h"

#include <QHBoxLayout>
#include <QEnterEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
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
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QRectF(1.0, 1.0, size.width() - 2.0, size.height() - 2.0));

    painter.setPen(Qt::white);
    QFont font("Microsoft YaHei UI", 10, QFont::DemiBold);
    font.setStyleStrategy(QFont::PreferAntialias);
    painter.setFont(font);
    painter.drawText(QRectF(0, 0, size.width(), size.height()), Qt::AlignCenter, name.left(1).toUpper());
    return pixmap;
}
}

ContactCell::ContactCell(QWidget *parent)
    : QWidget(parent)
    , _avatarLabel(new QLabel(this))
    , _nameLabel(new QLabel(this))
    , _messageLabel(new QLabel(this))
    , _timeLabel(new QLabel(this))
{
    auto *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(10, 6, 10, 6);
    rootLayout->setSpacing(8);

    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_Hover, true);
    _avatarLabel->setFixedSize(30, 30);
    _avatarLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    _nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    _messageLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    _timeLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    auto *textLayout = new QVBoxLayout;
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(1);
    textLayout->addWidget(_nameLabel);
    textLayout->addWidget(_messageLabel);

    rootLayout->addWidget(_avatarLabel, 0, Qt::AlignVCenter);
    rootLayout->addLayout(textLayout, 1);
    rootLayout->addWidget(_timeLabel, 0, Qt::AlignTop);

    setFixedHeight(cellHeight());
    setMouseTracking(true);
    updateStyles();
}

void ContactCell::setContact(const ContactItem &contact)
{
    _nameLabel->setText(contact.name);
    _messageLabel->setText(fontMetrics().elidedText(contact.lastMessage, Qt::ElideRight, 140));
    _timeLabel->setText(contact.timeText);
    updateAvatar(contact);
}

void ContactCell::setSelected(bool selected)
{
    if (_selected == selected) {
        return;
    }
    _selected = selected;
    updateStyles();
}

int ContactCell::cellHeight()
{
    return 61;
}

void ContactCell::mousePressEvent(QMouseEvent *event)
{
    QWidget::mousePressEvent(event);
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    }
}

void ContactCell::enterEvent(QEnterEvent *event)
{
    QWidget::enterEvent(event);
    _hovered = true;
    updateStyles();
}

void ContactCell::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    _hovered = false;
    updateStyles();
}

void ContactCell::updateAvatar(const ContactItem &contact)
{
    _avatarLabel->setPixmap(buildAvatarPixmap(contact.name, contact.avatarColor, _avatarLabel->size()));
    _avatarLabel->setContentsMargins(0, 2, 0, 0);
}

void ContactCell::updateStyles()
{
    QString background = "transparent";
    if (_selected) {
        background = "#E1E0E5";
    } else if (_hovered) {
        background = "#EAE9EF";
    }

    setStyleSheet(QString("background:%1; border:none; border-radius:12px;").arg(background));

    QFont nameFont("Microsoft YaHei UI", 9, QFont::DemiBold);
    QFont textFont("Microsoft YaHei UI", 8);
    nameFont.setStyleStrategy(QFont::PreferAntialias);
    textFont.setStyleStrategy(QFont::PreferAntialias);
    _nameLabel->setFont(nameFont);
    _messageLabel->setFont(textFont);
    _timeLabel->setFont(textFont);
    _nameLabel->setStyleSheet("color:#18212f; background:transparent;");
    _messageLabel->setStyleSheet("color:#6b7280; background:transparent;");
    _timeLabel->setStyleSheet("color:#94a3b8; background:transparent;");
}
