#ifndef MESSAGECELL_H
#define MESSAGECELL_H

#include <QWidget>
#include "chattypes.h"

class QLabel;
class QPushButton;
class QTextBrowser;

class MessageCell : public QWidget
{
    Q_OBJECT

public:
    explicit MessageCell(QWidget *parent = nullptr);
    ~MessageCell() override;

    void setMessage(const MessageItem &message, int availableWidth);
    static int heightForMessage(const MessageItem &message, int availableWidth);

signals:
    void retryRequested(const QString &clientMsgId);

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void updateAvatar(const MessageItem &message);
    void layoutOutgoing(int bubbleWidth, int contentHeight);
    void layoutIncoming(int bubbleWidth, int contentHeight);
    void showTextContextMenu(const QPoint &pos);
    void showImagePreview() const;
    void updateStatusWidget(const MessageItem &message, int bubbleX, int bubbleWidth, int contentHeight);

    QLabel *_avatarLabel;
    QWidget *_bubbleWidget;
    QTextBrowser *_textBrowser;
    QLabel *_imageLabel;
    QPushButton *_statusButton;
    QImage _currentImage;
    QString _currentClientMsgId;
};

#endif // MESSAGECELL_H
