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

#endif // CHATTYPES_H
