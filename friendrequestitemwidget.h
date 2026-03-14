#ifndef FRIENDREQUESTITEMWIDGET_H
#define FRIENDREQUESTITEMWIDGET_H

#include <QWidget>
#include "chattypes.h"

class QLabel;
class QPushButton;

class FriendRequestItemWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FriendRequestItemWidget(QWidget *parent = nullptr);

    void setRequestItem(const FriendRequestItem &item);

signals:
    void acceptClicked(int requestId);
    void rejectClicked(int requestId);

private:
    void updateAvatar(const FriendRequestItem &item);
    void updateStyles(const FriendRequestItem &item);

    int _requestId = 0;
    QLabel *_avatarLabel;
    QLabel *_nameLabel;
    QLabel *_detailLabel;
    QLabel *_sourceLabel;
    QLabel *_statusLabel;
    QPushButton *_acceptButton;
    QPushButton *_rejectButton;
};

#endif // FRIENDREQUESTITEMWIDGET_H
