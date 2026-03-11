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
    void appendMessage(const MessageItem &message);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void rebuildPool();
    void recalculateLayout();
    void updateVisibleCells();
    bool isNearBottom() const;
    void scrollToBottom();

    QWidget *_contentWidget;
    QVector<MessageItem> _messages;
    QVector<int> _offsets;
    QVector<int> _heights;
    QVector<MessageCell *> _cellPool;
    bool _autoFollowLatest = true;
};

#endif // MESSAGELISTWIDGET_H
