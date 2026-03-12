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
class SearchPopupWidget;
class QLineEdit;

class ChatPage : public QWidget
{
    Q_OBJECT

public:
    explicit ChatPage(QWidget *parent = nullptr);
    ~ChatPage();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onContactActivated(int index);
    void onSendClicked();
    void onMockReceiveClicked();
    void onImagePasted();
    void onSearchTextChanged(const QString &text);
    void onPopupAddFriendClicked(const QString &text);
    void onPopupContactClicked(int contactId);

private:
    struct Conversation {
        ContactItem contact;
        QVector<MessageItem> messages;
    };

    void setupUiExtensions();
    void setupNavigation();
    void setupMockData();
    void bindConversation(int index);
    void showSearchPopup();
    void hideSearchPopup();
    void updateSearchPopup();
    QVector<ContactItem> filteredContacts(const QString &text) const;
    int conversationIndexById(int contactId) const;
    void refreshContactSummaries();
    void syncContactList();
    void sortConversationsByLatest();
    int moveConversationToFront(int index);
    QDateTime latestTimestamp(const Conversation &conversation) const;
    MessageItem createOutgoingTextMessage(const QString &text);
    MessageItem createOutgoingImageMessage(const QImage &image);
    MessageItem createIncomingMockMessage();
    QString formatMessagePreview(const MessageItem &message) const;
    QString formatMessageTime(const QDateTime &timestamp) const;

    Ui::ChatPage *ui;
    ContactListWidget *_contactListWidget;
    MessageListWidget *_messageListWidget;
    ChatInputEdit *_chatInputEdit;
    SearchPopupWidget *_searchPopup;
    QVector<Conversation> _conversations;
    int _currentConversation = 0;
    int _messageIdSeed = 1000;
};

#endif // CHATPAGE_H
