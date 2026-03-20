#ifndef CHATPAGE_H
#define CHATPAGE_H

#include <QJsonArray>
#include <QJsonObject>
#include <QByteArray>
#include <QHash>
#include <QSet>
#include <QVector>
#include <QWidget>
#include "global.h"
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
class QLabel;
class QNetworkAccessManager;
class QPushButton;
class QNetworkReply;
class QScrollArea;
class QVBoxLayout;
class QWidget;

class ChatPage : public QWidget
{
    Q_OBJECT

public:
    explicit ChatPage(QWidget *parent = nullptr);
    ~ChatPage();
    void setCurrentUser(int uid, const QString &name);

signals:
    void friendRequestNotificationChanged(bool hasUnread);
    void chatMessageNotificationChanged(bool hasUnread);

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
    void onFriendRequestRejected(int requestId);
    void onSearchUserRsp(const QJsonObject &payload);
    void onAddFriendRsp(const QJsonObject &payload);
    void onFriendRequestsRsp(const QJsonObject &payload);
    void onHandleFriendRequestRsp(const QJsonObject &payload);
    void onFriendListPush(const QJsonObject &payload);
    void onPrivateMessagesRsp(const QJsonObject &payload);
    void onSendPrivateMessageRsp(const QJsonObject &payload);
    void onPrivateMessagePush(const QJsonObject &payload);
    void onChatHttpFinished(ReqId id, QString res, ErrorCodes err);
    void onRetryMessageRequested(const QString &clientMsgId);
    void onServerClosed();
    void onHistoryTopReached();

private:
    struct Conversation {
        ContactItem contact;
        QVector<MessageItem> messages;
        bool hasMoreHistory = true;
        bool loadingHistory = false;
    };

    struct PendingImageUpload {
        int contactId = 0;
        QString clientMsgId;
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
    QByteArray encodeImageForUpload(const QImage &image) const;
    void populateImageMessage(MessageItem &item) const;
    QString formatMessagePreview(const MessageItem &message) const;
    QString formatMessageTime(const QDateTime &timestamp) const;
    QColor avatarColorForName(const QString &name) const;
    void addOutgoingFriendRequest(const QString &name);
    void addOutgoingFriendRequest(const ContactItem &contact, const QString &remark = QString());
    void addIncomingFriendRequest(const QString &name);
    void refreshFriendRequestList();
    void applyFriendList(const QJsonArray &friends);
    MessageItem messageFromJson(const QJsonObject &obj) const;
    void applyPrivateMessages(int contactId, const QJsonArray &messages, bool incremental, bool prependHistory = false);
    void appendPrivateMessage(const QJsonObject &obj, bool moveToTop);
    void ensureConversationForFriend(FriendRequestItem &item);
    void restoreCurrentConversation(int contactId);
    void hydrateConversationMessages(Conversation &conversation);
    void ensureConversationMessagesLoaded(int index);
    void requestFriendList();
    void requestFriendRequests();
    void requestPrivateMessages(int contactId, int limit = 10, qint64 afterMsgId = -1);
    void requestOlderPrivateMessages(int contactId, qint64 beforeMsgId, int limit = 10);
    void requestMarkConversationRead(int contactId);
    bool resolveAddFriendTarget(const QString &text, ContactItem &contact) const;
    void updateNavigationIcons();
    void updateFriendRequestBadge();
    void updateChatBadge();
    void updateChatUnreadNotification();
    QString createClientMessageId() const;
    void appendPendingOutgoingMessage(int contactId, const MessageItem &message);
    bool updatePendingMessageState(const QString &clientMsgId, MessageSendState state, const QJsonObject *serverMessage = nullptr);
    bool retryMessageByClientId(const QString &clientMsgId);
    void markAllSendingMessagesFailed();
    QString normalizeContactPreview(const QString &text) const;
    QString normalizeImageResourceKey(const QString &text) const;
    QString imageCacheDirectory() const;
    QString localImageCachePath(const QString &resourceKey) const;
    void ensureImageAvailable(MessageItem &item);
    void cachePendingImage(const QString &clientMsgId, const QString &resourceKey);
    void requestImageDownload(const QString &resourceKey);
    void onImageDownloadFinished(QNetworkReply *reply);
    void refreshImageResource(const QString &resourceKey);

    Ui::ChatPage *ui;
    ContactListWidget *_contactListWidget;
    MessageListWidget *_messageListWidget;
    ChatInputEdit *_chatInputEdit;
    SearchPopupWidget *_searchPopup;
    QPushButton *_mockFriendRequestButton = nullptr;
    QScrollArea *_friendRequestScrollArea = nullptr;
    QWidget *_friendRequestListWidget = nullptr;
    QVBoxLayout *_friendRequestListLayout = nullptr;
    QVector<Conversation> _conversations;
    QVector<FriendRequestItem> _friendRequests;
    QVector<ContactItem> _searchResults;
    QHash<QString, PendingImageUpload> _pendingImageUploadTargets;
    QSet<int> _knownPendingIncomingRequestIds;
    ContactItem _pendingAddFriendTarget;
    QString _pendingAddFriendRemark;
    QLabel *_friendRequestBadgeLabel = nullptr;
    QLabel *_chatBadgeLabel = nullptr;
    QNetworkAccessManager *_imageDownloadManager = nullptr;
    bool _hasUnreadFriendRequestNotification = false;
    bool _hasUnreadChatNotification = false;
    bool _addFriendDialogActive = false;
    QSet<QString> _downloadingImageResources;
    int _currentUserId = 0;
    QString _currentUserName;
    int _currentConversation = 0;
    int _messageIdSeed = 1000;
    int _friendRequestIdSeed = 2000;
};

#endif // CHATPAGE_H
