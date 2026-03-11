#include "contactcell.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>

namespace {
QPixmap buildAvatarPixmap(const QString &name, const QColor &color, const QSize &size)
{
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(pixmap.rect().adjusted(1, 1, -1, -1));

    painter.setPen(Qt::white);
    painter.setFont(QFont("Microsoft YaHei UI", 12, QFont::DemiBold));
    painter.drawText(pixmap.rect(), Qt::AlignCenter, name.left(1).toUpper());
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
    rootLayout->setContentsMargins(16, 12, 16, 12);
    rootLayout->setSpacing(12);

    _avatarLabel->setFixedSize(48, 48);

    auto *textLayout = new QVBoxLayout;
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(4);
    textLayout->addWidget(_nameLabel);
    textLayout->addWidget(_messageLabel);

    rootLayout->addWidget(_avatarLabel, 0, Qt::AlignTop);
    rootLayout->addLayout(textLayout, 1);
    rootLayout->addWidget(_timeLabel, 0, Qt::AlignTop);

    setFixedHeight(cellHeight());
    updateStyles();
}

void ContactCell::setContact(const ContactItem &contact)
{
    _nameLabel->setText(contact.name);
    _messageLabel->setText(fontMetrics().elidedText(contact.lastMessage, Qt::ElideRight, 220));
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
    return 84;
}

void ContactCell::mousePressEvent(QMouseEvent *event)
{
    QWidget::mousePressEvent(event);
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    }
}

void ContactCell::updateAvatar(const ContactItem &contact)
{
    _avatarLabel->setPixmap(buildAvatarPixmap(contact.name, contact.avatarColor, _avatarLabel->size()));
}

void ContactCell::updateStyles()
{
    const QString background = _selected ? "#e8f0ff" : "transparent";
    const QString border = _selected ? "#d7e3ff" : "transparent";

    setStyleSheet(QString(
        "ContactCell { background:%1; border:1px solid %2; border-radius:18px; }"
        "QLabel { background:transparent; border:none; }")
        .arg(background, border));

    _nameLabel->setFont(QFont("Microsoft YaHei UI", 11, QFont::DemiBold));
    _messageLabel->setFont(QFont("Microsoft YaHei UI", 9));
    _timeLabel->setFont(QFont("Microsoft YaHei UI", 9));
    _nameLabel->setStyleSheet("color:#18212f;");
    _messageLabel->setStyleSheet("color:#6b7280;");
    _timeLabel->setStyleSheet("color:#94a3b8;");
}
