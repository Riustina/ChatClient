#ifndef CONTACTCELL_H
#define CONTACTCELL_H

#include <QWidget>
#include "chattypes.h"

class QLabel;
class QMouseEvent;
class QEnterEvent;

class ContactCell : public QWidget
{
    Q_OBJECT

public:
    explicit ContactCell(QWidget *parent = nullptr);

    void setContact(const ContactItem &contact);
    void setSelected(bool selected);

    static int cellHeight();

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateMessageText();
    void updateAvatar(const ContactItem &contact);
    void updateStyles();
    void updateUnreadBadge(const ContactItem &contact);

    QLabel *_avatarLabel;
    QLabel *_nameLabel;
    QLabel *_messageLabel;
    QLabel *_timeLabel;
    QLabel *_unreadBadgeLabel;
    QString _lastMessageText;
    bool _selected = false;
    bool _hovered = false;
};

#endif // CONTACTCELL_H
