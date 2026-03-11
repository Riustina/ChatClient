#ifndef CONTACTCELL_H
#define CONTACTCELL_H

#include <QWidget>
#include "chattypes.h"

class QLabel;
class QMouseEvent;

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

private:
    void updateAvatar(const ContactItem &contact);
    void updateStyles();

    QLabel *_avatarLabel;
    QLabel *_nameLabel;
    QLabel *_messageLabel;
    QLabel *_timeLabel;
    bool _selected = false;
};

#endif // CONTACTCELL_H
