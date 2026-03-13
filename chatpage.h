#ifndef CHATPAGE_H
#define CHATPAGE_H

#include <QJsonObject>
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
class FriendRequestItemWidget;
class MessageListWidget;
class SearchPopupWidget;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QTimer;
class QVBoxLayout;
class QWidget;

class ChatPage : public QWidget
{
    Q_OBJECT

public:
    explicit ChatPage(QWidget *parent = nullptr);
    ~ChatPage();
    void setCurrentUser(int uid, const QString &name);

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
    void onMockFriendRequestClicked();
    void onFriendRequestAccepted(int requestId);
    void onSearchUserRsp(const QJsonObject &payload);
    void onAddFriendRsp(const QJsonObject &payload);
    void onFriendRequestsRsp(const QJsonObject &payload);
    void onHandleFriendRequestRsp(const QJsonObject &payload);

private:
    struct Conversation {
        ContactItem contact;
        QVector<MessageItem> messages;
    };

    void setupUiExtensions();
    void setupNavigation();
    void setupMockData();
    void setupFriendRequestPage();
    void applyEmptyConversationState();
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
    QColor avatarColorForName(const QString &name) const;
    void addOutgoingFriendRequest(const QString &name);
    void addOutgoingFriendRequest(const ContactItem &contact);
    void addIncomingFriendRequest(const QString &name);
    void refreshFriendRequestList();
    void ensureConversationForFriend(FriendRequestItem &item);
    void restoreCurrentConversation(int contactId);
    void requestFriendRequests();
    bool resolveAddFriendTarget(const QString &text, ContactItem &contact) const;

    Ui::ChatPage *ui;
    ContactListWidget *_contactListWidget;
    MessageListWidget *_messageListWidget;
    ChatInputEdit *_chatInputEdit;
    SearchPopupWidget *_searchPopup;
    QTimer *_friendRequestPollTimer = nullptr;
    QPushButton *_mockFriendRequestButton = nullptr;
    QScrollArea *_friendRequestScrollArea = nullptr;
    QWidget *_friendRequestListWidget = nullptr;
    QVBoxLayout *_friendRequestListLayout = nullptr;
    QVector<Conversation> _conversations;
    QVector<FriendRequestItem> _friendRequests;
    QVector<ContactItem> _searchResults;
    ContactItem _pendingAddFriendTarget;
    int _currentUserId = 0;
    QString _currentUserName;
    int _currentConversation = 0;
    int _messageIdSeed = 1000;
    int _friendRequestIdSeed = 2000;
};

#endif // CHATPAGE_H
