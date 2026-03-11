#ifndef CHATPAGE_H
#define CHATPAGE_H

#include <QVector>
#include <QWidget>
#include "chattypes.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class ChatPage;
}
QT_END_NAMESPACE

class ChatInputEdit;
class ContactListWidget;
class MessageListWidget;

class ChatPage : public QWidget
{
    Q_OBJECT

public:
    explicit ChatPage(QWidget *parent = nullptr);
    ~ChatPage();

private slots:
    void onContactActivated(int index);
    void onSendClicked();
    void onMockReceiveClicked();
    void onImagePasted();

private:
    struct Conversation {
        ContactItem contact;
        QVector<MessageItem> messages;
    };

    void setupUiExtensions();
    void setupNavigation();
    void setupMockData();
    void bindConversation(int index);
    void refreshContactSummaries();
    void syncContactList();
    MessageItem createOutgoingTextMessage(const QString &text);
    MessageItem createOutgoingImageMessage(const QImage &image);
    MessageItem createIncomingMockMessage();
    QString formatMessagePreview(const MessageItem &message) const;
    QString formatMessageTime(const QDateTime &timestamp) const;

    Ui::ChatPage *ui;
    ContactListWidget *_contactListWidget;
    MessageListWidget *_messageListWidget;
    ChatInputEdit *_chatInputEdit;
    QVector<Conversation> _conversations;
    int _currentConversation = 0;
    int _messageIdSeed = 1000;
};

#endif // CHATPAGE_H
