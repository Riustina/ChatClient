#ifndef MESSAGELISTWIDGET_H
#define MESSAGELISTWIDGET_H

#include <QScrollArea>
#include <QVector>
#include "chattypes.h"

class MessageCell;
class QResizeEvent;

class MessageListWidget : public QScrollArea
{
    Q_OBJECT

public:
    explicit MessageListWidget(QWidget *parent = nullptr);

    void setMessages(const QVector<MessageItem> &messages);
    void refreshMessagesPreservePosition(const QVector<MessageItem> &messages);
    void appendMessage(const MessageItem &message);
    void prependMessages(const QVector<MessageItem> &messages);

signals:
    void retryRequested(const QString &clientMsgId);
    void reachedTop();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void rebuildPool();
    void recalculateLayout();
    void updateVisibleCells();
    bool isNearBottom() const;
    void scrollToBottom();
    bool isNearTop() const;

    QWidget *_contentWidget;
    QVector<MessageItem> _messages;
    QVector<int> _offsets;
    QVector<int> _heights;
    QVector<MessageCell *> _cellPool;
    bool _autoFollowLatest = true;
    bool _topSignalArmed = true;
};

#endif // MESSAGELISTWIDGET_H
