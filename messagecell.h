#ifndef MESSAGECELL_H
#define MESSAGECELL_H

#include <QWidget>
#include "chattypes.h"

class QLabel;
class QTextBrowser;

class MessageCell : public QWidget
{
    Q_OBJECT

public:
    explicit MessageCell(QWidget *parent = nullptr);
    ~MessageCell() override;

    void setMessage(const MessageItem &message, int availableWidth);
    static int heightForMessage(const MessageItem &message, int availableWidth);

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void updateAvatar(const MessageItem &message);
    void layoutOutgoing(int bubbleWidth, int contentHeight);
    void layoutIncoming(int bubbleWidth, int contentHeight);
    void showTextContextMenu(const QPoint &pos);
    void showImagePreview() const;

    QLabel *_avatarLabel;
    QWidget *_bubbleWidget;
    QTextBrowser *_textBrowser;
    QLabel *_imageLabel;
    QImage _currentImage;
};

#endif // MESSAGECELL_H
