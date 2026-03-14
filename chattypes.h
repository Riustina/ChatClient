#ifndef CHATTYPES_H
#define CHATTYPES_H

#include <QColor>
#include <QDateTime>
#include <QImage>
#include <QString>

enum class ChatMessageType {
    Text,
    Image
};

enum class FriendRequestDirection {
    Outgoing,
    Incoming
};

enum class FriendRequestState {
    Pending,
    Added,
    Rejected
};

struct ContactItem {
    int id = 0;
    QString name;
    QString lastMessage;
    QString timeText;
    QColor avatarColor;
};

struct MessageItem {
    int id = 0;
    QString senderName;
    bool outgoing = false;
    ChatMessageType type = ChatMessageType::Text;
    QString text;
    QImage image;
    QColor avatarColor;
    QDateTime timestamp;
};

struct FriendRequestItem {
    int id = 0;
    int contactId = 0;
    QString name;
    QString remark;
    QColor avatarColor;
    FriendRequestDirection direction = FriendRequestDirection::Outgoing;
    FriendRequestState state = FriendRequestState::Pending;
    QDateTime createdAt;
};

#endif // CHATTYPES_H
