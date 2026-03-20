#include "chatpage.h"
#include "ui_chatpage.h"

#include "addfrienddialog.h"
#include "chatinputedit.h"
#include "contactlistwidget.h"
#include "friendrequestitemwidget.h"
#include "httpmgr.h"
#include "localdb.h"
#include "messagelistwidget.h"
#include "searchpopupwidget.h"
#include "tcpmgr.h"

#include <QApplication>
#include <QButtonGroup>
#include <QBuffer>
#include <QDateTime>
#include <QEvent>
#include <QDir>
#include <QFocusEvent>
#include <QFile>
#include <QFileInfo>
#include <QLineEdit>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QStyle>
#include <algorithm>

namespace {
QColor avatarColorForIndex(int index)
{
    static const QList<QColor> colors = {
        QColor("#4f46e5"),
        QColor("#0f766e"),
        QColor("#ea580c"),
        QColor("#dc2626"),
        QColor("#2563eb"),
        QColor("#7c3aed")
    };
    return colors[index % colors.size()];
}

const char *kChatNavIconPath = ":/icons/nav_chat.png";
const char *kFriendRequestNavIconPath = ":/icons/nav_friend_request.png";
}

ChatPage::ChatPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ChatPage)
    , _contactListWidget(new ContactListWidget(this))
    , _messageListWidget(new MessageListWidget(this))
    , _chatInputEdit(new ChatInputEdit(this))
    , _searchPopup(new SearchPopupWidget(this))
    , _imageDownloadManager(new QNetworkAccessManager(this))
{
    ui->setupUi(this);

    setupUiExtensions();
    setupNavigation();
    setupMockData();
    setupFriendRequestPage();
    sortConversationsByLatest();
    refreshContactSummaries();
    syncContactList();
    applyEmptyConversationState();
}

ChatPage::~ChatPage()
{
    qApp->removeEventFilter(this);
    delete ui;
}

void ChatPage::setCurrentUser(int uid, const QString &name)
{
    _currentUserId = uid;
    _currentUserName = name;
    _conversations.clear();
    _friendRequests.clear();
    _searchResults.clear();
    _currentConversation = 0;
    _knownPendingIncomingRequestIds.clear();
    _hasUnreadFriendRequestNotification = false;
    _hasUnreadChatNotification = false;

    const QVector<ContactItem> localContacts = LocalDb::instance().loadFriendList();
    qDebug() << "[ChatPage] setCurrentUser 从本地数据库恢复好友列表:" << localContacts.size() << "条";
    _conversations.reserve(localContacts.size());
    for (const ContactItem &contact : localContacts) {
        Conversation conversation;
        conversation.contact = contact;
        conversation.contact.lastMessage = normalizeContactPreview(conversation.contact.lastMessage);
        conversation.contact.avatarColor = avatarColorForName(
            conversation.contact.name.isEmpty() ? QString::number(conversation.contact.id) : conversation.contact.name);
        const QVector<MessageItem> localMessages = LocalDb::instance().loadConversationMessages(conversation.contact.id, uid);
        if (!localMessages.isEmpty()) {
            const MessageItem &lastMessage = localMessages.back();
            conversation.contact.lastMessage = normalizeContactPreview(formatMessagePreview(lastMessage));
            conversation.contact.timeText = formatMessageTime(lastMessage.timestamp);
        }
        _conversations.push_back(conversation);
    }

    _friendRequests = LocalDb::instance().loadFriendRequests(uid);
    qDebug() << "[ChatPage] setCurrentUser 从本地数据库恢复好友申请:" << _friendRequests.size() << "条";
    for (FriendRequestItem &request : _friendRequests) {
        request.avatarColor = avatarColorForName(request.name);
    }

    if (!_conversations.isEmpty()) {
        _conversations[0].messages = LocalDb::instance().loadConversationMessages(_conversations[0].contact.id, _currentUserId);
        hydrateConversationMessages(_conversations[0]);
        qDebug() << "[ChatPage] setCurrentUser 从本地数据库恢复默认会话消息:" << _conversations[0].messages.size()
                 << "条, contactId:" << _conversations[0].contact.id;
        syncContactList();
        bindConversation(0);
    } else {
        qDebug() << "[ChatPage] setCurrentUser 本地数据库没有可恢复的好友列表";
        syncContactList();
        applyEmptyConversationState();
    }

    refreshFriendRequestList();
    updateFriendRequestBadge();
    updateChatBadge();
    emit friendRequestNotificationChanged(false);
    emit chatMessageNotificationChanged(false);
    requestFriendList();
    requestFriendRequests();
}

bool ChatPage::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->searchLineEdit) {
        if (event->type() == QEvent::FocusIn) {
            showSearchPopup();
        } else if (event->type() == QEvent::MouseButtonPress) {
            showSearchPopup();
        }
    }

    if (_searchPopup->isVisible() && event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        const QPoint globalPos = mouseEvent->globalPosition().toPoint();
        const bool inSearch = ui->searchLineEdit->rect().contains(ui->searchLineEdit->mapFromGlobal(globalPos));
        const QRect popupRect(_searchPopup->mapToGlobal(QPoint(0, 0)), _searchPopup->size());
        const bool inPopup = popupRect.contains(globalPos);
        if (!inSearch && !inPopup) {
            hideSearchPopup();
        }
    }

    return QWidget::eventFilter(watched, event);
}

void ChatPage::onContactActivated(int index)
{
    ui->chatNavButton->setChecked(true);
    ui->rightStackedWidget->setCurrentIndex(0);
    ensureConversationMessagesLoaded(index);
    bindConversation(index);
    syncContactList();
    if (index >= 0 && index < _conversations.size()) {
        requestPrivateMessages(_conversations[index].contact.id);
    }
}

void ChatPage::onSendClicked()
{
    if (_conversations.isEmpty() || _currentConversation < 0 || _currentConversation >= _conversations.size()) {
        return;
    }

    const int contactId = _conversations[_currentConversation].contact.id;
    const QImage pastedImage = _chatInputEdit->takePastedImage();
    const QString text = _chatInputEdit->plainTextForSend().trimmed();
    if (text.isEmpty() && pastedImage.isNull()) {
        return;
    }

    const bool chatAvailable = TcpMgr::getInstance().isChatAvailable();

    if (!pastedImage.isNull()) {
        MessageItem message = createOutgoingImageMessage(pastedImage);
        appendPendingOutgoingMessage(contactId, message);
        const QByteArray encodedImage = encodeImageForUpload(pastedImage);
        if (encodedImage.isEmpty()) {
            updatePendingMessageState(message.clientMsgId, MessageSendState::Failed);
            QMessageBox::warning(this,
                                 QString::fromUtf8(u8"\u53d1\u9001\u5931\u8d25"),
                                 QString::fromUtf8(u8"\u56fe\u7247\u8fc7\u5927\uff0c\u8bf7\u5c1d\u8bd5\u66f4\u5c0f\u7684\u56fe\u7247\u3002"));
            return;
        }

        const QString uploadId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        PendingImageUpload pendingUpload;
        pendingUpload.contactId = contactId;
        pendingUpload.clientMsgId = message.clientMsgId;
        _pendingImageUploadTargets.insert(uploadId, pendingUpload);
        QJsonObject imageObj;
        imageObj["upload_id"] = uploadId;
        imageObj["content_encoding"] = "zlib+png";
        imageObj["content"] = QString::fromLatin1(encodedImage);
        HttpMgr::getInstance().PostHttpReq(
            QUrl(gate_url_prefix + "/upload_image"),
            imageObj,
            ReqId::ID_UPLOAD_IMAGE,
            Modules::CHATMOD);
    }
    if (!text.isEmpty()) {
        MessageItem message = createOutgoingTextMessage(text);
        appendPendingOutgoingMessage(contactId, message);
        if (!chatAvailable) {
            updatePendingMessageState(message.clientMsgId, MessageSendState::Failed);
        } else {
        QJsonObject obj;
        obj["to_uid"] = contactId;
        obj["content_type"] = "text";
        obj["content"] = text;
        obj["client_msg_id"] = message.clientMsgId;
        emit TcpMgr::getInstance().sig_send_data(ID_SEND_PRIVATE_MESSAGE_REQ, QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
        }
    }

    _chatInputEdit->clear();
    _chatInputEdit->setPlaceholderText(QStringLiteral("输入消息，Enter 发送，Shift+Enter 换行。"));
}

void ChatPage::onMockReceiveClicked()
{
    if (_conversations.isEmpty() || _currentConversation < 0 || _currentConversation >= _conversations.size()) {
        return;
    }

    _conversations[_currentConversation].messages.push_back(createIncomingMockMessage());
    _currentConversation = moveConversationToFront(_currentConversation);
    refreshContactSummaries();
    syncContactList();
    bindConversation(_currentConversation);
}

void ChatPage::onImagePasted()
{
    if (_chatInputEdit->hasPendingImage()) {
        _chatInputEdit->setPlaceholderText(QString::fromUtf8(u8"\u5df2\u63d2\u5165\u56fe\u7247\u9884\u89c8\uff0c\u53ef\u76f4\u63a5\u9000\u683c\u5220\u9664\u3002"));
    }
}

void ChatPage::onSearchTextChanged(const QString &text)
{
    Q_UNUSED(text);
    _searchResults.clear();
    updateSearchPopup();
}

void ChatPage::onPopupAddFriendClicked(const QString &text)
{
    hideSearchPopup();
    ui->searchLineEdit->clearFocus();

    const QString keyword = text.trimmed();
    if (keyword.isEmpty()) {
        return;
    }

    _searchResults.clear();
    QJsonObject obj;
    obj["keyword"] = keyword;
    obj["limit"] = 20;
    emit TcpMgr::getInstance().sig_send_data(ID_SEARCH_USER_REQ, QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void ChatPage::onPopupContactClicked(int contactId)
{
    const int index = conversationIndexById(contactId);
    if (index >= 0) {
        bindConversation(index);
        syncContactList();
        hideSearchPopup();
        return;
    }

    ContactItem targetContact;
    for (const ContactItem &item : std::as_const(_searchResults)) {
        if (item.id == contactId) {
            targetContact = item;
            break;
        }
    }
    if (targetContact.id <= 0) {
        hideSearchPopup();
        return;
    }

    for (const FriendRequestItem &request : std::as_const(_friendRequests)) {
        if (request.contactId != targetContact.id) {
            continue;
        }

        if (request.state == FriendRequestState::Added) {
            QMessageBox::information(this,
                                     QString::fromUtf8(u8"\u63d0\u793a"),
                                     QString::fromUtf8(u8"\u4f60\u4eec\u5df2\u7ecf\u662f\u597d\u53cb\u4e86\u3002"));
            hideSearchPopup();
            return;
        }

        if (request.state == FriendRequestState::Pending) {
            QMessageBox::information(this,
                                     QString::fromUtf8(u8"\u63d0\u793a"),
                                     QString::fromUtf8(u8"\u4f60\u5df2\u7ecf\u5411\u5bf9\u65b9\u53d1\u9001\u8fc7\u597d\u53cb\u7533\u8bf7\uff0c\u8bf7\u7b49\u5f85\u5904\u7406\u3002"));
            hideSearchPopup();
            return;
        }
    }

    _pendingAddFriendTarget = targetContact;
    AddFriendDialog dialog(targetContact.name, this);
    if (dialog.exec() == QDialog::Accepted) {
        _pendingAddFriendRemark = dialog.remark();
        QJsonObject obj;
        obj["to_uid"] = targetContact.id;
        obj["remark"] = _pendingAddFriendRemark;
        emit TcpMgr::getInstance().sig_send_data(ID_ADD_FRIEND_REQ, QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
    }
    hideSearchPopup();
}

void ChatPage::setupUiExtensions()
{
    ui->contactListLayout->addWidget(_contactListWidget);
    ui->messageListLayout->addWidget(_messageListWidget);
    ui->inputLayout->addWidget(_chatInputEdit);

    updateNavigationIcons();
    ui->navFrame->setFixedWidth(52);
    ui->contactFrame->setFixedWidth(255);
    ui->chatNavButton->setIconSize(QSize(18, 18));
    ui->friendRequestNavButton->setIconSize(QSize(18, 18));
    ui->chatNavButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    ui->friendRequestNavButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    ui->chatNavButton->setFixedSize(30, 30);
    ui->friendRequestNavButton->setFixedSize(30, 30);
    ui->navLayout->setAlignment(ui->chatNavButton, Qt::AlignHCenter);
    ui->navLayout->setAlignment(ui->friendRequestNavButton, Qt::AlignHCenter);
    ui->composerFrame->setMinimumHeight(165);
    ui->composerFrame->setMaximumHeight(195);

    _friendRequestBadgeLabel = new QLabel(ui->friendRequestNavButton);
    _friendRequestBadgeLabel->setFixedSize(8, 8);
    _friendRequestBadgeLabel->move(ui->friendRequestNavButton->width() - 10, 2);
    _friendRequestBadgeLabel->setStyleSheet("background:#ef4444; border-radius:4px;");
    _friendRequestBadgeLabel->hide();

    _chatBadgeLabel = new QLabel(ui->chatNavButton);
    _chatBadgeLabel->setFixedSize(8, 8);
    _chatBadgeLabel->move(ui->chatNavButton->width() - 10, 2);
    _chatBadgeLabel->setStyleSheet("background:#ef4444; border-radius:4px;");
    _chatBadgeLabel->hide();

    setStyleSheet(
        "ChatPage { background:#F4F3F9; }"
        "QFrame#navFrame { background:#F4F3F9; border-right:1px solid #dfdde7; }"
        "QFrame#contactFrame { background:#F4F3F9; border-right:1px solid #dfdde7; }"
        "QFrame#rightFrame { background:#F4F3F9; }"
        "QFrame#chatHeaderFrame { background:#F4F3F9; border-bottom:1px solid #e4e2eb; }"
        "QFrame#composerFrame { background:#F4F3F9; border-top:1px solid #e4e2eb; }"
        "QFrame#searchFrame { background:#EAE9EF; border-radius:17px; }"
        "QLineEdit#searchLineEdit { background:transparent; border:none; padding:0 14px; font: 10pt 'Microsoft YaHei UI'; color:#1f2937; }"
        "QToolButton#chatNavButton, QToolButton#friendRequestNavButton { min-width:30px; max-width:30px; min-height:30px; max-height:30px; padding:0px; border:none; border-radius:9px; color:#7b7a82; background:transparent; }"
        "QToolButton#chatNavButton:hover:!checked, QToolButton#friendRequestNavButton:hover:!checked { background:#EAE9EF; color:#1f2937; }"
        "QToolButton#chatNavButton:checked, QToolButton#friendRequestNavButton:checked { background:#CBCACF; color:#1f2937; }"
        "QLabel#chatTitleLabel { font: 13pt 'Microsoft YaHei UI'; color:#111827; }"
        "QToolButton#headerActionButton1, QToolButton#headerActionButton2, QToolButton#headerActionButton3 { background:#EAE9EF; border:none; border-radius:12px; padding:4px 10px; color:#334155; font: 9pt 'Microsoft YaHei UI'; }"
        "QToolButton#headerActionButton1:pressed, QToolButton#headerActionButton2:pressed, QToolButton#headerActionButton3:pressed { background:#d9d6df; padding-top:5px; padding-left:11px; }"
        "QPushButton#sendButton, QPushButton#mockReceiveButton { min-width:82px; min-height:34px; border:none; border-radius:17px; font: 10pt 'Microsoft YaHei UI'; }"
        "QPushButton#mockReceiveButton { background:#EAE9EF; color:#334155; }"
        "QPushButton#sendButton { background:#CBCACF; color:#111827; }"
        "QPushButton#mockReceiveButton:pressed { background:#d9d6df; padding-top:1px; }"
        "QPushButton#sendButton:pressed { background:#b9b7be; padding-top:1px; }"
        "QPushButton#mockFriendRequestButton { min-width:110px; min-height:34px; border:none; border-radius:17px; background:#CBCACF; color:#111827; font: 10pt 'Microsoft YaHei UI'; }"
        "QPushButton#mockFriendRequestButton:pressed { background:#b9b7be; padding-top:1px; }"
        "QTextEdit { background:#F4F3F9; border:none; border-radius:18px; padding:12px; font: 10pt 'Microsoft YaHei UI'; }"
        "QLabel#friendRequestTitleLabel { font: 18pt 'Microsoft YaHei UI'; color:#111827; }"
        "QLabel#friendRequestHintLabel { font: 11pt 'Microsoft YaHei UI'; color:#64748b; }"
        "QScrollArea#friendRequestScrollArea { background:transparent; border:none; }"
        "QScrollBar:vertical { background:transparent; width:8px; margin:6px 2px 6px 0px; }"
        "QScrollBar::handle:vertical { background:#d9d2e2; border-radius:4px; min-height:30px; }"
        "QScrollBar::handle:vertical:hover { background:#c8bfd3; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background:transparent; }");

    _chatInputEdit->setPlaceholderText(QStringLiteral("输入消息，Enter 发送，Shift+Enter 换行。"));
    ui->searchLineEdit->setPlaceholderText(QStringLiteral("搜索联系人 / 添加好友"));

    _searchPopup->hide();
    ui->searchLineEdit->installEventFilter(this);
    qApp->installEventFilter(this);

    connect(_contactListWidget, &ContactListWidget::contactActivated, this, &ChatPage::onContactActivated);
    connect(ui->sendButton, &QPushButton::clicked, this, &ChatPage::onSendClicked);
    connect(ui->mockReceiveButton, &QPushButton::clicked, this, &ChatPage::onMockReceiveClicked);
    connect(_chatInputEdit, &ChatInputEdit::imagePasted, this, &ChatPage::onImagePasted);
    connect(_chatInputEdit, &ChatInputEdit::sendRequested, this, &ChatPage::onSendClicked);
    connect(ui->searchLineEdit, &QLineEdit::textChanged, this, &ChatPage::onSearchTextChanged);
    connect(_searchPopup, &SearchPopupWidget::addFriendClicked, this, &ChatPage::onPopupAddFriendClicked);
    connect(_searchPopup, &SearchPopupWidget::contactClicked, this, &ChatPage::onPopupContactClicked);
    connect(_imageDownloadManager, &QNetworkAccessManager::finished, this, &ChatPage::onImageDownloadFinished);
    connect(&TcpMgr::getInstance(), &TcpMgr::sig_search_user_rsp, this, &ChatPage::onSearchUserRsp);
    connect(&TcpMgr::getInstance(), &TcpMgr::sig_add_friend_rsp, this, &ChatPage::onAddFriendRsp);
    connect(&TcpMgr::getInstance(), &TcpMgr::sig_friend_requests_rsp, this, &ChatPage::onFriendRequestsRsp);
    connect(&TcpMgr::getInstance(), &TcpMgr::sig_handle_friend_request_rsp, this, &ChatPage::onHandleFriendRequestRsp);
    connect(&TcpMgr::getInstance(), &TcpMgr::sig_friend_list_push, this, &ChatPage::onFriendListPush);
    connect(&TcpMgr::getInstance(), &TcpMgr::sig_private_messages_rsp, this, &ChatPage::onPrivateMessagesRsp);
    connect(&TcpMgr::getInstance(), &TcpMgr::sig_send_private_message_rsp, this, &ChatPage::onSendPrivateMessageRsp);
    connect(&TcpMgr::getInstance(), &TcpMgr::sig_private_message_push, this, &ChatPage::onPrivateMessagePush);
    connect(&TcpMgr::getInstance(), &TcpMgr::sig_server_closed, this, &ChatPage::onServerClosed);
    connect(&HttpMgr::getInstance(), &HttpMgr::sig_chat_mod_http_finished, this, &ChatPage::onChatHttpFinished);
    connect(_messageListWidget, &MessageListWidget::retryRequested, this, &ChatPage::onRetryMessageRequested);
}

void ChatPage::setupFriendRequestPage()
{
    ui->friendRequestHintLabel->setText(QStringLiteral("这里展示你发起的好友申请，以及别人向你发来的好友申请。"));

    _mockFriendRequestButton = new QPushButton(QStringLiteral("模拟收到申请"), this);
    _mockFriendRequestButton->setObjectName("mockFriendRequestButton");
    _mockFriendRequestButton->hide();

    _friendRequestScrollArea = new QScrollArea(this);
    _friendRequestScrollArea->setObjectName("friendRequestScrollArea");
    _friendRequestScrollArea->setFrameShape(QFrame::NoFrame);
    _friendRequestScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _friendRequestScrollArea->setWidgetResizable(true);

    _friendRequestListWidget = new QWidget(_friendRequestScrollArea);
    _friendRequestListWidget->setAttribute(Qt::WA_StyledBackground, true);
    _friendRequestListWidget->setStyleSheet("background:transparent;");
    _friendRequestListLayout = new QVBoxLayout(_friendRequestListWidget);
    _friendRequestListLayout->setContentsMargins(0, 0, 6, 0);
    _friendRequestListLayout->setSpacing(10);
    _friendRequestListLayout->addStretch();

    _friendRequestScrollArea->setWidget(_friendRequestListWidget);

    ui->friendRequestLayout->insertWidget(2, _mockFriendRequestButton, 0, Qt::AlignLeft);
    ui->friendRequestLayout->insertWidget(3, _friendRequestScrollArea, 1);

    connect(_mockFriendRequestButton, &QPushButton::clicked, this, &ChatPage::onMockFriendRequestClicked);
    refreshFriendRequestList();
}

void ChatPage::setupNavigation()
{
    auto *group = new QButtonGroup(this);
    group->setExclusive(true);
    group->addButton(ui->chatNavButton, 0);
    group->addButton(ui->friendRequestNavButton, 1);

    connect(group, &QButtonGroup::idClicked, this, [this](int id) {
        ui->rightStackedWidget->setCurrentIndex(id == 0 ? 0 : 1);
        if (id == 1) {
            _hasUnreadFriendRequestNotification = false;
            updateFriendRequestBadge();
            emit friendRequestNotificationChanged(false);
        } else if (id == 0) {
            updateChatUnreadNotification();
        }
    });

    ui->chatNavButton->setChecked(true);
    ui->rightStackedWidget->setCurrentIndex(0);
}

void ChatPage::setupMockData()
{
    _conversations.clear();
    _currentConversation = 0;
}

void ChatPage::bindConversation(int index)
{
    if (index < 0 || index >= _conversations.size()) {
        applyEmptyConversationState();
        return;
    }

    ensureConversationMessagesLoaded(index);
    _currentConversation = index;
    _conversations[index].contact.unreadCount = 0;
    ui->chatTitleLabel->setText(_conversations[index].contact.name);
    _messageListWidget->setMessages(_conversations[index].messages);
    syncContactList();
    updateChatUnreadNotification();
    requestMarkConversationRead(_conversations[index].contact.id);
}

void ChatPage::applyEmptyConversationState()
{
    ui->chatTitleLabel->setText(QStringLiteral("暂无会话"));
    _messageListWidget->setMessages({});
}

void ChatPage::showSearchPopup()
{
    updateSearchPopup();
    const QPoint belowSearch = mapFromGlobal(ui->searchLineEdit->mapToGlobal(QPoint(0, ui->searchLineEdit->height() + 6)));
    const int width = ui->searchLineEdit->width();
    _searchPopup->resize(width, _searchPopup->popupHeight());
    _searchPopup->move(belowSearch);
    _searchPopup->show();
    _searchPopup->raise();
}

void ChatPage::hideSearchPopup()
{
    _searchPopup->hide();
}

void ChatPage::updateSearchPopup()
{
    const QString text = ui->searchLineEdit->text().trimmed();
    _searchPopup->setSearchText(text);
    const int currentContactId = (_conversations.isEmpty() || _currentConversation < 0 || _currentConversation >= _conversations.size())
        ? -1
        : _conversations[_currentConversation].contact.id;
    _searchPopup->setResults(filteredContacts(text), currentContactId);
}

QVector<ContactItem> ChatPage::filteredContacts(const QString &text) const
{
    if (text.isEmpty()) {
        return {};
    }
    return _searchResults;
}

int ChatPage::conversationIndexById(int contactId) const
{
    for (int i = 0; i < _conversations.size(); ++i) {
        if (_conversations[i].contact.id == contactId) {
            return i;
        }
    }
    return -1;
}

void ChatPage::refreshContactSummaries()
{
    for (Conversation &conversation : _conversations) {
        if (conversation.messages.isEmpty()) {
            continue;
        }

        const MessageItem &last = conversation.messages.back();
        conversation.contact.lastMessage = normalizeContactPreview(formatMessagePreview(last));
        conversation.contact.timeText = formatMessageTime(last.timestamp);
    }
}

void ChatPage::syncContactList()
{
    QVector<ContactItem> contacts;
    contacts.reserve(_conversations.size());
    for (const Conversation &conversation : _conversations) {
        contacts.push_back(conversation.contact);
    }
    const int currentContactId = _conversations.isEmpty() ? -1 : _conversations[_currentConversation].contact.id;
    _contactListWidget->setContacts(contacts, currentContactId);
}

void ChatPage::sortConversationsByLatest()
{
    std::stable_sort(_conversations.begin(), _conversations.end(), [this](const Conversation &lhs, const Conversation &rhs) {
        return latestTimestamp(lhs) > latestTimestamp(rhs);
    });
}

int ChatPage::moveConversationToFront(int index)
{
    if (index <= 0 || index >= _conversations.size()) {
        return qBound(0, index, qMax(0, _conversations.size() - 1));
    }

    const Conversation conversation = _conversations.takeAt(index);
    _conversations.prepend(conversation);
    return 0;
}

QDateTime ChatPage::latestTimestamp(const Conversation &conversation) const
{
    if (conversation.messages.isEmpty()) {
        return QDateTime();
    }

    return conversation.messages.back().timestamp;
}

MessageItem ChatPage::createOutgoingTextMessage(const QString &text)
{
    MessageItem message;
    message.id = -++_messageIdSeed;
    message.clientMsgId = createClientMessageId();
    message.senderName = _currentUserName.isEmpty() ? QStringLiteral("我") : _currentUserName;
    message.outgoing = true;
    message.type = ChatMessageType::Text;
    message.sendState = MessageSendState::Sending;
    message.text = text;
    message.avatarColor = QColor("#111827");
    message.timestamp = QDateTime::currentDateTime();
    return message;
}

MessageItem ChatPage::createOutgoingImageMessage(const QImage &image)
{
    MessageItem message;
    message.id = -++_messageIdSeed;
    message.clientMsgId = createClientMessageId();
    message.senderName = _currentUserName.isEmpty() ? QStringLiteral("我") : _currentUserName;
    message.outgoing = true;
    message.type = ChatMessageType::Image;
    message.sendState = MessageSendState::Sending;
    message.image = image;
    message.avatarColor = QColor("#111827");
    message.timestamp = QDateTime::currentDateTime();
    return message;
}

QByteArray ChatPage::encodeImageForUpload(const QImage &image) const
{
    if (image.isNull()) {
        return {};
    }

    constexpr int kMaxEncodedBytes = 8 * 1024 * 1024;
    const QImage normalized = image.convertToFormat(QImage::Format_RGBA8888);

    QByteArray pngBytes;
    {
        QBuffer pngBuffer(&pngBytes);
        pngBuffer.open(QIODevice::WriteOnly);
        if (!normalized.save(&pngBuffer, "PNG")) {
            return {};
        }
    }

    const QByteArray compressed = qCompress(pngBytes, 9);
    const QByteArray encoded = compressed.toBase64();
    if (encoded.size() <= kMaxEncodedBytes) {
        return encoded;
    }

    return {};
}

void ChatPage::populateImageMessage(MessageItem &item) const
{
    if (item.type != ChatMessageType::Image || !item.image.isNull()) {
        return;
    }

    const_cast<ChatPage *>(this)->ensureImageAvailable(item);
}

MessageItem ChatPage::createIncomingMockMessage()
{
    static const QStringList texts = {
        QStringLiteral("我看过了，列表滚动方向和布局关系都没问题。"),
        QStringLiteral("你接下来可以补一下搜索弹层入口。"),
        QStringLiteral("我再发一轮测试数据，你看看气泡和间距。")
    };

    MessageItem message;
    message.id = ++_messageIdSeed;
    message.senderName = _conversations[_currentConversation].contact.name;
    message.outgoing = false;
    message.type = ChatMessageType::Text;
    message.text = texts[QRandomGenerator::global()->bounded(texts.size())];
    message.avatarColor = _conversations[_currentConversation].contact.avatarColor;
    message.timestamp = QDateTime::currentDateTime();
    return message;
}

QString ChatPage::formatMessagePreview(const MessageItem &message) const
{
    if (message.type == ChatMessageType::Image) {
        return QString::fromUtf8(u8"[\u56fe\u7247]");
    }
    return message.text;
}

QString ChatPage::formatMessageTime(const QDateTime &timestamp) const
{
    return timestamp.time().toString("HH:mm");
}

void ChatPage::onMockFriendRequestClicked()
{
    static const QStringList names = {
        QStringLiteral("林夏"),
        QStringLiteral("周屿"),
        QStringLiteral("程宁"),
        QStringLiteral("沈青"),
        QStringLiteral("许言"),
        QStringLiteral("顾南")
    };

    const QString name = names[QRandomGenerator::global()->bounded(names.size())];
    addIncomingFriendRequest(name);
}

void ChatPage::onFriendRequestAccepted(int requestId)
{
    QJsonObject obj;
    obj["request_id"] = requestId;
    obj["accept"] = true;
    emit TcpMgr::getInstance().sig_send_data(ID_HANDLE_FRIEND_REQUEST_REQ, QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void ChatPage::onFriendRequestRejected(int requestId)
{
    QJsonObject obj;
    obj["request_id"] = requestId;
    obj["accept"] = false;
    emit TcpMgr::getInstance().sig_send_data(ID_HANDLE_FRIEND_REQUEST_REQ, QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

QColor ChatPage::avatarColorForName(const QString &name) const
{
    int seed = 0;
    for (const QChar ch : name) {
        seed += ch.unicode();
    }
    return avatarColorForIndex(seed);
}

void ChatPage::addOutgoingFriendRequest(const QString &name)
{
    if (name.isEmpty()) {
        return;
    }

    int existingContactId = 0;
    for (const Conversation &conversation : _conversations) {
        if (conversation.contact.name == name) {
            existingContactId = conversation.contact.id;
            break;
        }
    }

    FriendRequestItem request;
    request.id = ++_friendRequestIdSeed;
    request.contactId = existingContactId;
    request.name = name;
    request.avatarColor = avatarColorForName(name);
    request.direction = FriendRequestDirection::Outgoing;
    request.state = existingContactId > 0 ? FriendRequestState::Added : FriendRequestState::Pending;
    request.createdAt = QDateTime::currentDateTime();
    _friendRequests.prepend(request);
    refreshFriendRequestList();
}

void ChatPage::addOutgoingFriendRequest(const ContactItem &contact, const QString &remark)
{
    if (contact.id <= 0 || contact.name.isEmpty()) {
        return;
    }

    for (const FriendRequestItem &item : _friendRequests) {
        if (item.contactId == contact.id && item.direction == FriendRequestDirection::Outgoing
            && item.state == FriendRequestState::Pending) {
            return;
        }
    }

    FriendRequestItem request;
    request.id = ++_friendRequestIdSeed;
    request.contactId = contact.id;
    request.name = contact.name;
    request.remark = remark.trimmed();
    request.avatarColor = contact.avatarColor;
    request.direction = FriendRequestDirection::Outgoing;
    request.state = FriendRequestState::Pending;
    request.createdAt = QDateTime::currentDateTime();
    _friendRequests.prepend(request);
    refreshFriendRequestList();
}

void ChatPage::addIncomingFriendRequest(const QString &name)
{
    int existingContactId = 0;
    for (const Conversation &conversation : _conversations) {
        if (conversation.contact.name == name) {
            existingContactId = conversation.contact.id;
            break;
        }
    }

    FriendRequestItem request;
    request.id = ++_friendRequestIdSeed;
    request.contactId = existingContactId;
    request.name = name;
    request.avatarColor = avatarColorForName(name);
    request.direction = FriendRequestDirection::Incoming;
    request.state = FriendRequestState::Pending;
    request.createdAt = QDateTime::currentDateTime();
    _friendRequests.prepend(request);
    refreshFriendRequestList();
}

void ChatPage::refreshFriendRequestList()
{
    while (QLayoutItem *item = _friendRequestListLayout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            delete widget;
        }
        delete item;
    }

    for (const FriendRequestItem &request : _friendRequests) {
        auto *itemWidget = new FriendRequestItemWidget(_friendRequestListWidget);
        itemWidget->setRequestItem(request);
        connect(itemWidget, &FriendRequestItemWidget::acceptClicked, this, &ChatPage::onFriendRequestAccepted);
        connect(itemWidget, &FriendRequestItemWidget::rejectClicked, this, &ChatPage::onFriendRequestRejected);
        _friendRequestListLayout->addWidget(itemWidget);
    }

    _friendRequestListLayout->addStretch();
}

void ChatPage::applyFriendList(const QJsonArray &friends)
{
    const int currentContactId = (_currentConversation >= 0 && _currentConversation < _conversations.size())
        ? _conversations[_currentConversation].contact.id
        : -1;

    QHash<int, Conversation> existingById;
    for (const Conversation &conversation : std::as_const(_conversations)) {
        existingById.insert(conversation.contact.id, conversation);
    }

    QVector<Conversation> rebuilt;
    rebuilt.reserve(friends.size());
    for (const QJsonValue &value : friends) {
        const QJsonObject obj = value.toObject();
        const int uid = obj.value("uid").toInt();
        if (uid <= 0) {
            continue;
        }

        Conversation conversation = existingById.value(uid);
        conversation.contact.id = uid;
        conversation.contact.name = obj.value("name").toString();
        conversation.contact.lastMessage = normalizeContactPreview(obj.value("last_message").toString());
        conversation.contact.timeText = obj.value("last_time").toString();
        conversation.contact.unreadCount = obj.value("unread_count").toInt();
        conversation.contact.avatarColor = avatarColorForName(
            conversation.contact.name.isEmpty() ? QString::number(uid) : conversation.contact.name);
        rebuilt.push_back(conversation);
    }

    _conversations = rebuilt;
    QVector<ContactItem> contactsToSave;
    contactsToSave.reserve(_conversations.size());
    for (const Conversation &conversation : std::as_const(_conversations)) {
        contactsToSave.push_back(conversation.contact);
    }
    if (!LocalDb::instance().saveFriendList(contactsToSave)) {
        qWarning() << "[ChatPage] 保存好友列表到本地数据库失败:" << LocalDb::instance().lastError();
    }
    refreshContactSummaries();
    if (!_conversations.isEmpty()) {
        sortConversationsByLatest();
        restoreCurrentConversation(currentContactId);
        syncContactList();
        bindConversation(_currentConversation);
        requestPrivateMessages(_conversations[_currentConversation].contact.id);
    } else {
        syncContactList();
        applyEmptyConversationState();
    }
}

void ChatPage::requestFriendList()
{
    if (_currentUserId <= 0) {
        return;
    }

    QJsonObject obj;
    const QString cursor = LocalDb::instance().syncValue(QStringLiteral("friend_list_cursor"));
    if (!cursor.isEmpty()) {
        obj["updated_after"] = cursor;
    }
    emit TcpMgr::getInstance().sig_send_data(ID_GET_FRIEND_LIST_REQ, QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void ChatPage::requestFriendRequests()
{
    if (_currentUserId <= 0) {
        return;
    }

    QJsonObject obj;
    const QString cursor = LocalDb::instance().syncValue(QStringLiteral("friend_request_cursor"));
    if (!cursor.isEmpty()) {
        obj["updated_after"] = cursor;
    }
    emit TcpMgr::getInstance().sig_send_data(ID_GET_FRIEND_REQUESTS_REQ, QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

bool ChatPage::resolveAddFriendTarget(const QString &text, ContactItem &contact) const
{
    bool ok = false;
    const int uid = text.toInt(&ok);
    if (ok) {
        for (const ContactItem &item : _searchResults) {
            if (item.id == uid) {
                contact = item;
                return true;
            }
        }
    }

    ContactItem exactMatch;
    int exactCount = 0;
    for (const ContactItem &item : _searchResults) {
        if (item.name.compare(text, Qt::CaseInsensitive) == 0) {
            exactMatch = item;
            ++exactCount;
        }
    }

    if (exactCount == 1) {
        contact = exactMatch;
        return true;
    }
    return false;
}

void ChatPage::onSearchUserRsp(const QJsonObject &payload)
{
    _searchResults.clear();
    const QJsonArray users = payload.value("users").toArray();
    for (const QJsonValue &value : users) {
        const QJsonObject obj = value.toObject();
        ContactItem contact;
        contact.id = obj.value("uid").toInt();
        contact.name = obj.value("name").toString();
        contact.avatarColor = avatarColorForName(contact.name.isEmpty() ? QString::number(contact.id) : contact.name);
        _searchResults.push_back(contact);
    }
    updateSearchPopup();
    showSearchPopup();
}

void ChatPage::onAddFriendRsp(const QJsonObject &payload)
{
    if (payload.value("error").toInt() != 0) {
        const QString message = payload.value("message").toString().trimmed();
        QMessageBox::warning(this,
                             QString::fromUtf8(u8"\u6dfb\u52a0\u597d\u53cb\u5931\u8d25"),
                             message.isEmpty() ? QString::fromUtf8(u8"\u5f53\u524d\u65e0\u6cd5\u53d1\u9001\u597d\u53cb\u7533\u8bf7\uff0c\u8bf7\u7a0d\u540e\u518d\u8bd5\u3002") : message);
        _pendingAddFriendTarget = ContactItem{};
        _pendingAddFriendRemark.clear();
        return;
    }

    _pendingAddFriendTarget = ContactItem{};
    _pendingAddFriendRemark.clear();
}

void ChatPage::onFriendRequestsRsp(const QJsonObject &payload)
{
    if (payload.value("error").toInt() != 0) {
        return;
    }

    const bool incremental = payload.value("incremental").toBool(false);
    const QString cursor = payload.value("cursor").toString();
    qDebug() << "[ChatPage] onFriendRequestsRsp 收到服务端好友申请列表:"
             << payload.value("requests").toArray().size() << "条, incremental:" << incremental;

    QVector<FriendRequestItem> mergedRequests = incremental ? _friendRequests : QVector<FriendRequestItem>{};
    QHash<int, int> requestIndexById;
    for (int i = 0; i < mergedRequests.size(); ++i) {
        requestIndexById.insert(mergedRequests[i].id, i);
    }
    QSet<int> currentPendingIncomingIds;
    const QJsonArray requests = payload.value("requests").toArray();
    for (const QJsonValue &value : requests) {
        const QJsonObject obj = value.toObject();
        FriendRequestItem item;
        item.id = obj.value("request_id").toInt();
        const int fromUid = obj.value("from_uid").toInt();
        const int toUid = obj.value("to_uid").toInt();
        const QString status = obj.value("status").toString();
        const bool outgoing = fromUid == _currentUserId;
        item.contactId = outgoing ? toUid : fromUid;
        item.name = outgoing ? obj.value("to_name").toString() : obj.value("from_name").toString();
        item.remark = obj.value("remark").toString();
        item.avatarColor = avatarColorForName(item.name);
        item.direction = outgoing ? FriendRequestDirection::Outgoing : FriendRequestDirection::Incoming;
        if (status == QStringLiteral("accepted")) {
            item.state = FriendRequestState::Added;
        } else if (status == QStringLiteral("rejected")) {
            item.state = FriendRequestState::Rejected;
        } else {
            item.state = FriendRequestState::Pending;
        }
        if (!outgoing && item.state == FriendRequestState::Pending) {
            currentPendingIncomingIds.insert(item.id);
        }
        item.createdAt = QDateTime::currentDateTime();
        if (requestIndexById.contains(item.id)) {
            mergedRequests[requestIndexById.value(item.id)] = item;
        } else {
            requestIndexById.insert(item.id, mergedRequests.size());
            mergedRequests.push_back(item);
        }
        _friendRequestIdSeed = qMax(_friendRequestIdSeed, item.id);
    }

    if (incremental) {
        for (const FriendRequestItem &item : std::as_const(mergedRequests)) {
            if (item.direction == FriendRequestDirection::Incoming && item.state == FriendRequestState::Pending) {
                currentPendingIncomingIds.insert(item.id);
            }
        }
    }
    _friendRequests = mergedRequests;

    if (ui->rightStackedWidget->currentIndex() != 1) {
        for (int requestId : currentPendingIncomingIds) {
            if (!_knownPendingIncomingRequestIds.contains(requestId)) {
                _hasUnreadFriendRequestNotification = true;
                break;
            }
        }
    } else {
        _hasUnreadFriendRequestNotification = false;
    }
    _knownPendingIncomingRequestIds = currentPendingIncomingIds;
    if (!LocalDb::instance().saveFriendRequests(_friendRequests, _currentUserId)) {
        qWarning() << "[ChatPage] 保存好友申请到本地数据库失败:" << LocalDb::instance().lastError();
    }
    if (!cursor.isEmpty()) {
        LocalDb::instance().setSyncValue(QStringLiteral("friend_request_cursor"), cursor);
    }
    updateFriendRequestBadge();
    emit friendRequestNotificationChanged(_hasUnreadFriendRequestNotification);
    refreshFriendRequestList();
}

void ChatPage::onHandleFriendRequestRsp(const QJsonObject &payload)
{
    if (payload.value("error").toInt() != 0) {
        return;
    }

    const int requestId = payload.value("request_id").toInt();
    const int currentContactId = (_currentConversation >= 0 && _currentConversation < _conversations.size())
        ? _conversations[_currentConversation].contact.id
        : -1;

    for (FriendRequestItem &request : _friendRequests) {
        if (request.id != requestId) {
            continue;
        }
        request.state = payload.value("accept").toBool() ? FriendRequestState::Added : FriendRequestState::Rejected;
        if (request.state == FriendRequestState::Added) {
            ensureConversationForFriend(request);
        }
        break;
    }

    refreshContactSummaries();
    if (!_conversations.isEmpty()) {
        sortConversationsByLatest();
        restoreCurrentConversation(currentContactId);
        syncContactList();
        bindConversation(_currentConversation);
    } else {
        syncContactList();
        applyEmptyConversationState();
    }
    refreshFriendRequestList();
}

void ChatPage::onFriendListPush(const QJsonObject &payload)
{
    if (payload.value("error").toInt() != 0) {
        return;
    }

    const bool incremental = payload.value("incremental").toBool(false);
    const QString cursor = payload.value("cursor").toString();
    qDebug() << "[ChatPage] onFriendListPush 收到服务端好友列表:"
             << payload.value("friends").toArray().size() << "条, incremental:" << incremental;

    QJsonArray friends = payload.value("friends").toArray();
    if (incremental) {
        QMap<int, QJsonObject> merged;
        for (const Conversation &conversation : std::as_const(_conversations)) {
            QJsonObject obj;
            obj["uid"] = conversation.contact.id;
            obj["name"] = conversation.contact.name;
            obj["last_message"] = conversation.contact.lastMessage;
            obj["last_time"] = conversation.contact.timeText;
            obj["unread_count"] = conversation.contact.unreadCount;
            merged.insert(conversation.contact.id, obj);
        }
        for (const QJsonValue &value : friends) {
            const QJsonObject obj = value.toObject();
            merged.insert(obj.value("uid").toInt(), obj);
        }

        friends = QJsonArray();
        for (auto it = merged.cbegin(); it != merged.cend(); ++it) {
            friends.push_back(it.value());
        }
    }

    applyFriendList(friends);
    if (!cursor.isEmpty()) {
        LocalDb::instance().setSyncValue(QStringLiteral("friend_list_cursor"), cursor);
    }
}

MessageItem ChatPage::messageFromJson(const QJsonObject &obj) const
{
    MessageItem item;
    item.id = obj.value("msg_id").toInt();
    item.senderName = obj.value("from_name").toString();
    item.outgoing = obj.value("from_uid").toInt() == _currentUserId;
    item.type = (obj.value("content_type").toString() == QStringLiteral("image"))
        ? ChatMessageType::Image
        : ChatMessageType::Text;
    item.text = obj.value("content").toString();
    item.avatarColor = avatarColorForName(item.senderName.isEmpty()
        ? QString::number(obj.value("from_uid").toInt())
        : item.senderName);
    item.timestamp = QDateTime::fromString(obj.value("created_at").toString(), Qt::ISODate);
    if (!item.timestamp.isValid()) {
        item.timestamp = QDateTime::fromString(obj.value("created_at").toString(), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    }
    if (!item.timestamp.isValid()) {
        item.timestamp = QDateTime::currentDateTime();
    }
    populateImageMessage(item);
    return item;
}

void ChatPage::applyPrivateMessages(int contactId, const QJsonArray &messages, bool incremental)
{
    const int index = conversationIndexById(contactId);
    if (index < 0) {
        return;
    }

    QVector<MessageItem> rebuilt;
    rebuilt.reserve(messages.size());
    for (const QJsonValue &value : messages) {
        rebuilt.push_back(messageFromJson(value.toObject()));
    }

    if (!incremental) {
        _conversations[index].messages = rebuilt;
    } else {
        QSet<int> existingIds;
        existingIds.reserve(_conversations[index].messages.size());
        for (const MessageItem &item : std::as_const(_conversations[index].messages)) {
            existingIds.insert(item.id);
        }
        for (const MessageItem &item : std::as_const(rebuilt)) {
            if (!existingIds.contains(item.id)) {
                _conversations[index].messages.push_back(item);
            }
            if (!LocalDb::instance().upsertMessage(contactId, item, _currentUserId)) {
                qWarning() << "[ChatPage] 淇濆瓨鍗曟潯澧為噺娑堟伅鍒版湰鍦版暟鎹簱澶辫触:" << LocalDb::instance().lastError();
            }
        }
    }
    if (!incremental && !LocalDb::instance().replaceConversationMessages(contactId, rebuilt, _currentUserId)) {
        qWarning() << "[ChatPage] 保存会话历史到本地数据库失败:" << LocalDb::instance().lastError();
    }
    hydrateConversationMessages(_conversations[index]);
    if (_currentConversation == index) {
        _conversations[index].contact.unreadCount = 0;
    }
    refreshContactSummaries();
    syncContactList();
    updateChatUnreadNotification();
    if (_currentConversation == index) {
        bindConversation(index);
    }
}

void ChatPage::appendPrivateMessage(const QJsonObject &obj, bool moveToTop)
{
    const int contactId = obj.value("contact_id").toInt();
    if (contactId <= 0) {
        return;
    }

    const int previousCurrentContactId = (_currentConversation >= 0 && _currentConversation < _conversations.size())
        ? _conversations[_currentConversation].contact.id
        : -1;

    int index = conversationIndexById(contactId);
    if (index < 0) {
        Conversation conversation;
        conversation.contact.id = contactId;
        conversation.contact.name = obj.value("from_uid").toInt() == _currentUserId
            ? obj.value("to_name").toString()
            : obj.value("from_name").toString();
        conversation.contact.avatarColor = avatarColorForName(conversation.contact.name);
        _conversations.prepend(conversation);
        index = 0;
    }

    const MessageItem message = messageFromJson(obj);
    _conversations[index].messages.push_back(message);
    hydrateConversationMessages(_conversations[index]);
    if (!LocalDb::instance().upsertMessage(contactId, message, _currentUserId)) {
        qWarning() << "[ChatPage] 保存单条消息到本地数据库失败:" << LocalDb::instance().lastError();
    }
    if (contactId != previousCurrentContactId) {
        ++_conversations[index].contact.unreadCount;
    } else {
        _conversations[index].contact.unreadCount = 0;
    }
    refreshContactSummaries();
    if (moveToTop) {
        index = moveConversationToFront(index);
    }
    if (contactId == previousCurrentContactId) {
        _currentConversation = index;
    } else {
        restoreCurrentConversation(previousCurrentContactId);
    }
    syncContactList();
    updateChatUnreadNotification();
    if (contactId == previousCurrentContactId) {
        bindConversation(index);
    } else if (_currentConversation >= 0 && _currentConversation < _conversations.size()) {
        bindConversation(_currentConversation);
    }
}

void ChatPage::requestPrivateMessages(int contactId, int limit, qint64 afterMsgId)
{
    if (_currentUserId <= 0 || contactId <= 0) {
        return;
    }

    if (afterMsgId < 0) {
        afterMsgId = LocalDb::instance().conversationCursor(contactId);
    }

    QJsonObject obj;
    obj["contact_id"] = contactId;
    obj["limit"] = limit;
    if (afterMsgId > 0) {
        obj["after_msg_id"] = afterMsgId;
    }
    emit TcpMgr::getInstance().sig_send_data(ID_GET_PRIVATE_MESSAGES_REQ, QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void ChatPage::requestMarkConversationRead(int contactId)
{
    if (_currentUserId <= 0 || contactId <= 0) {
        return;
    }

    QJsonObject obj;
    obj["contact_id"] = contactId;
    emit TcpMgr::getInstance().sig_send_data(ID_MARK_PRIVATE_MESSAGES_READ_REQ, QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void ChatPage::onPrivateMessagesRsp(const QJsonObject &payload)
{
    if (payload.value("error").toInt() != 0) {
        return;
    }

    const int contactId = payload.value("contact_id").toInt();
    const bool incremental = payload.value("incremental").toBool(false);
    const qint64 afterMsgId = payload.value("after_msg_id").toVariant().toLongLong();
    Q_UNUSED(afterMsgId);
    qDebug() << "[ChatPage] onPrivateMessagesRsp 收到服务端历史消息:"
             << payload.value("messages").toArray().size() << "条, contactId:" << contactId;
    applyPrivateMessages(contactId, payload.value("messages").toArray(), incremental);
}

void ChatPage::onSendPrivateMessageRsp(const QJsonObject &payload)
{
    const QString clientMsgId = payload.value("client_msg_id").toString();
    if (payload.value("error").toInt() != 0) {
        if (!clientMsgId.isEmpty()) {
            updatePendingMessageState(clientMsgId, MessageSendState::Failed);
        }
        const QString message = payload.value("message").toString().trimmed();
        QMessageBox::warning(this,
                             QString::fromUtf8(u8"\u53d1\u9001\u5931\u8d25"),
                             message.isEmpty() ? QString::fromUtf8(u8"\u8bf7\u91cd\u8bd5\u3002") : message);
        return;
    }

    const QJsonObject serverMessage = payload.value("message").toObject();
    if (!clientMsgId.isEmpty() && updatePendingMessageState(clientMsgId, MessageSendState::Sent, &serverMessage)) {
        return;
    }
    appendPrivateMessage(serverMessage, true);
}

void ChatPage::onPrivateMessagePush(const QJsonObject &payload)
{
    if (payload.value("error").toInt() != 0) {
        return;
    }

    appendPrivateMessage(payload.value("message").toObject(), true);
}

void ChatPage::onChatHttpFinished(ReqId id, QString res, ErrorCodes err)
{
    if (id != ReqId::ID_UPLOAD_IMAGE) {
        return;
    }

    auto failUpload = [this](const QString &uploadId) {
        if (uploadId.isEmpty() || !_pendingImageUploadTargets.contains(uploadId)) {
            return;
        }
        const PendingImageUpload pending = _pendingImageUploadTargets.take(uploadId);
        if (!pending.clientMsgId.isEmpty()) {
            updatePendingMessageState(pending.clientMsgId, MessageSendState::Failed);
        }
    };

    auto failAllPendingUploads = [this]() {
        const QList<PendingImageUpload> pendings = _pendingImageUploadTargets.values();
        _pendingImageUploadTargets.clear();
        for (const PendingImageUpload &pending : pendings) {
            if (!pending.clientMsgId.isEmpty()) {
                updatePendingMessageState(pending.clientMsgId, MessageSendState::Failed);
            }
        }
    };

    if (err != ErrorCodes::SUCCESS) {
        failAllPendingUploads();
        QMessageBox::warning(this,
                             QString::fromUtf8(u8"\u4e0a\u4f20\u5931\u8d25"),
                             QString::fromUtf8(u8"\u56fe\u7247\u4e0a\u4f20\u5931\u8d25\uff0c\u8bf7\u7a0d\u540e\u518d\u8bd5\u3002"));
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(res.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        failAllPendingUploads();
        QMessageBox::warning(this,
                             QString::fromUtf8(u8"\u4e0a\u4f20\u5931\u8d25"),
                             QString::fromUtf8(u8"\u56fe\u7247\u4e0a\u4f20\u56de\u5305\u89e3\u6790\u5931\u8d25\u3002"));
        return;
    }

    const QJsonObject obj = doc.object();
    const QString uploadId = obj.value("upload_id").toString();
    if (obj.value("error").toInt() != ErrorCodes::SUCCESS) {
        failUpload(uploadId);
        QMessageBox::warning(this,
                             QString::fromUtf8(u8"\u4e0a\u4f20\u5931\u8d25"),
                             obj.value("message").toString(QString::fromUtf8(u8"\u56fe\u7247\u4e0a\u4f20\u5931\u8d25\uff0c\u8bf7\u7a0d\u540e\u518d\u8bd5\u3002")));
        return;
    }

    const QString resourceKey = normalizeImageResourceKey(
        obj.value("resource_key").toString().isEmpty()
            ? obj.value("path").toString()
            : obj.value("resource_key").toString());
    if (uploadId.isEmpty() || resourceKey.isEmpty() || !_pendingImageUploadTargets.contains(uploadId)) {
        failUpload(uploadId);
        QMessageBox::warning(this,
                             QString::fromUtf8(u8"\u4e0a\u4f20\u5931\u8d25"),
                             QString::fromUtf8(u8"\u56fe\u7247\u4e0a\u4f20\u7ed3\u679c\u65e0\u6548\u3002"));
        return;
    }

    const PendingImageUpload pending = _pendingImageUploadTargets.take(uploadId);
    cachePendingImage(pending.clientMsgId, resourceKey);
    if (!TcpMgr::getInstance().isChatAvailable()) {
        updatePendingMessageState(pending.clientMsgId, MessageSendState::Failed);
        QMessageBox::warning(this,
                             QString::fromUtf8(u8"\u53d1\u9001\u5931\u8d25"),
                             QString::fromUtf8(u8"\u804a\u5929\u670d\u52a1\u5668\u8fde\u63a5\u5df2\u65ad\u5f00\uff0c\u8bf7\u91cd\u65b0\u767b\u5f55\u540e\u91cd\u8bd5\u3002"));
        return;
    }

    QJsonObject sendObj;
    sendObj["to_uid"] = pending.contactId;
    sendObj["content_type"] = "image";
    sendObj["content"] = resourceKey;
    sendObj["client_msg_id"] = pending.clientMsgId;
    emit TcpMgr::getInstance().sig_send_data(
        ID_SEND_PRIVATE_MESSAGE_REQ,
        QString::fromUtf8(QJsonDocument(sendObj).toJson(QJsonDocument::Compact)));
}

void ChatPage::ensureConversationForFriend(FriendRequestItem &item)
{
    if (conversationIndexById(item.contactId) >= 0) {
        return;
    }

    int nextId = 1;
    for (const Conversation &conversation : _conversations) {
        nextId = qMax(nextId, conversation.contact.id + 1);
    }
    if (item.contactId <= 0) {
        item.contactId = nextId;
    }

    Conversation conversation;
    conversation.contact.id = item.contactId;
    conversation.contact.name = item.name;
    conversation.contact.avatarColor = item.avatarColor;
    conversation.contact.lastMessage = QStringLiteral("我们已经成为好友");
    conversation.contact.timeText = QDateTime::currentDateTime().time().toString("HH:mm");

    MessageItem welcome;
    welcome.id = ++_messageIdSeed;
    welcome.senderName = item.name;
    welcome.outgoing = false;
    welcome.type = ChatMessageType::Text;
    welcome.text = QStringLiteral("你好，我们现在已经是好友了。");
    welcome.avatarColor = item.avatarColor;
    welcome.timestamp = QDateTime::currentDateTime();
    conversation.messages.push_back(welcome);

    _conversations.prepend(conversation);
}

void ChatPage::restoreCurrentConversation(int contactId)
{
    if (contactId < 0) {
        _currentConversation = qBound(0, _currentConversation, qMax(0, _conversations.size() - 1));
        return;
    }

    const int restoredIndex = conversationIndexById(contactId);
    if (restoredIndex >= 0) {
        _currentConversation = restoredIndex;
        return;
    }

    _currentConversation = qBound(0, _currentConversation, qMax(0, _conversations.size() - 1));
}

void ChatPage::hydrateConversationMessages(Conversation &conversation)
{
    const QString contactName = conversation.contact.name;
    const QColor incomingColor = conversation.contact.avatarColor.isValid()
        ? conversation.contact.avatarColor
        : avatarColorForName(contactName.isEmpty() ? QString::number(conversation.contact.id) : contactName);
    const QColor outgoingColor = avatarColorForName(_currentUserName.isEmpty()
        ? QString::number(_currentUserId)
        : _currentUserName);

    for (MessageItem &message : conversation.messages) {
        if (message.outgoing) {
            if (message.senderName.isEmpty()) {
                message.senderName = _currentUserName;
            }
            message.avatarColor = outgoingColor;
        } else {
            if (message.senderName.isEmpty()) {
                message.senderName = contactName;
            }
            message.avatarColor = incomingColor;
        }
        populateImageMessage(message);
    }
}

void ChatPage::ensureConversationMessagesLoaded(int index)
{
    if (index < 0 || index >= _conversations.size()) {
        return;
    }

    if (!_conversations[index].messages.isEmpty()) {
        hydrateConversationMessages(_conversations[index]);
        return;
    }

    _conversations[index].messages = LocalDb::instance().loadConversationMessages(_conversations[index].contact.id, _currentUserId);
    hydrateConversationMessages(_conversations[index]);
    qDebug() << "[ChatPage] ensureConversationMessagesLoaded 从本地数据库恢复会话消息:"
             << _conversations[index].messages.size() << "条, contactId:" << _conversations[index].contact.id;
}

void ChatPage::updateNavigationIcons()
{
    const QIcon chatIcon = QIcon(QString::fromLatin1(kChatNavIconPath));
    const QIcon requestIcon = QIcon(QString::fromLatin1(kFriendRequestNavIconPath));

    ui->chatNavButton->setIcon(chatIcon.isNull() ? style()->standardIcon(QStyle::SP_FileDialogDetailedView) : chatIcon);
    ui->friendRequestNavButton->setIcon(requestIcon.isNull() ? style()->standardIcon(QStyle::SP_DialogApplyButton) : requestIcon);
}

void ChatPage::updateFriendRequestBadge()
{
    if (_friendRequestBadgeLabel == nullptr) {
        return;
    }
    _friendRequestBadgeLabel->setVisible(_hasUnreadFriendRequestNotification);
    _friendRequestBadgeLabel->raise();
}

void ChatPage::updateChatBadge()
{
    if (_chatBadgeLabel == nullptr) {
        return;
    }
    _chatBadgeLabel->setVisible(_hasUnreadChatNotification);
    _chatBadgeLabel->raise();
}

void ChatPage::updateChatUnreadNotification()
{
    bool hasUnread = false;
    for (const Conversation &conversation : std::as_const(_conversations)) {
        if (conversation.contact.unreadCount > 0) {
            hasUnread = true;
            break;
        }
    }

    if (_hasUnreadChatNotification != hasUnread) {
        _hasUnreadChatNotification = hasUnread;
        emit chatMessageNotificationChanged(hasUnread);
    }
    updateChatBadge();
}

void ChatPage::appendPendingOutgoingMessage(int contactId, const MessageItem &message)
{
    int index = conversationIndexById(contactId);
    if (index < 0) {
        return;
    }

    _conversations[index].messages.push_back(message);
    hydrateConversationMessages(_conversations[index]);
    refreshContactSummaries();
    index = moveConversationToFront(index);
    _currentConversation = index;
    syncContactList();
    bindConversation(index);

    const QString clientMsgId = message.clientMsgId;
    QTimer::singleShot(10000, this, [this, clientMsgId]() {
        updatePendingMessageState(clientMsgId, MessageSendState::Failed);
    });
}

bool ChatPage::updatePendingMessageState(const QString &clientMsgId, MessageSendState state, const QJsonObject *serverMessage)
{
    if (clientMsgId.isEmpty()) {
        return false;
    }

    for (int i = 0; i < _conversations.size(); ++i) {
        for (MessageItem &message : _conversations[i].messages) {
            if (message.clientMsgId != clientMsgId) {
                continue;
            }

            if (state == MessageSendState::Failed && message.sendState != MessageSendState::Sending && serverMessage == nullptr) {
                return false;
            }
            message.sendState = state;
            if (serverMessage != nullptr) {
                MessageItem confirmed = messageFromJson(*serverMessage);
                confirmed.clientMsgId = clientMsgId;
                confirmed.sendState = MessageSendState::Sent;
                if (message.type == ChatMessageType::Image && confirmed.image.isNull()) {
                    confirmed.image = message.image;
                }
                message = confirmed;
                LocalDb::instance().upsertMessage(_conversations[i].contact.id, message, _currentUserId);
            }

            refreshContactSummaries();
            syncContactList();
            if (_currentConversation == i) {
                bindConversation(i);
            }
            return true;
        }
    }

    return false;
}

bool ChatPage::retryMessageByClientId(const QString &clientMsgId)
{
    if (clientMsgId.isEmpty()) {
        return false;
    }

    for (int i = 0; i < _conversations.size(); ++i) {
        for (MessageItem &message : _conversations[i].messages) {
            if (message.clientMsgId != clientMsgId || !message.outgoing || message.sendState != MessageSendState::Failed) {
                continue;
            }

            const QString newClientMsgId = createClientMessageId();
            message.clientMsgId = newClientMsgId;
            message.sendState = MessageSendState::Sending;
            message.timestamp = QDateTime::currentDateTime();

            const int contactId = _conversations[i].contact.id;
            const bool chatAvailable = TcpMgr::getInstance().isChatAvailable();
            if (message.type == ChatMessageType::Image) {
                if (!message.text.isEmpty()) {
                    if (!chatAvailable) {
                        message.sendState = MessageSendState::Failed;
                    } else {
                        QJsonObject sendObj;
                        sendObj["to_uid"] = contactId;
                        sendObj["content_type"] = "image";
                        sendObj["content"] = message.text;
                        sendObj["client_msg_id"] = message.clientMsgId;
                        emit TcpMgr::getInstance().sig_send_data(
                            ID_SEND_PRIVATE_MESSAGE_REQ,
                            QString::fromUtf8(QJsonDocument(sendObj).toJson(QJsonDocument::Compact)));
                    }
                } else {
                    const QByteArray encodedImage = encodeImageForUpload(message.image);
                    if (encodedImage.isEmpty()) {
                        message.sendState = MessageSendState::Failed;
                        return false;
                    }
                    const QString uploadId = QUuid::createUuid().toString(QUuid::WithoutBraces);
                    PendingImageUpload pendingUpload;
                    pendingUpload.contactId = contactId;
                    pendingUpload.clientMsgId = message.clientMsgId;
                    _pendingImageUploadTargets.insert(uploadId, pendingUpload);

                    QJsonObject imageObj;
                    imageObj["upload_id"] = uploadId;
                    imageObj["content_encoding"] = "zlib+png";
                    imageObj["content"] = QString::fromLatin1(encodedImage);
                    HttpMgr::getInstance().PostHttpReq(
                        QUrl(gate_url_prefix + "/upload_image"),
                        imageObj,
                        ReqId::ID_UPLOAD_IMAGE,
                        Modules::CHATMOD);
                }
            } else {
                if (!chatAvailable) {
                    message.sendState = MessageSendState::Failed;
                } else {
                    QJsonObject obj;
                    obj["to_uid"] = contactId;
                    obj["content_type"] = "text";
                    obj["content"] = message.text;
                    obj["client_msg_id"] = message.clientMsgId;
                    emit TcpMgr::getInstance().sig_send_data(
                        ID_SEND_PRIVATE_MESSAGE_REQ,
                        QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
                }
            }

            QTimer::singleShot(10000, this, [this, newClientMsgId]() {
                updatePendingMessageState(newClientMsgId, MessageSendState::Failed);
            });

            if (_currentConversation == i) {
                bindConversation(i);
            } else {
                syncContactList();
            }
            return true;
        }
    }

    return false;
}

void ChatPage::onRetryMessageRequested(const QString &clientMsgId)
{
    retryMessageByClientId(clientMsgId);
}

void ChatPage::markAllSendingMessagesFailed()
{
    bool changed = false;
    for (Conversation &conversation : _conversations) {
        for (MessageItem &message : conversation.messages) {
            if (message.outgoing && message.sendState == MessageSendState::Sending) {
                message.sendState = MessageSendState::Failed;
                changed = true;
            }
        }
    }

    if (changed) {
        refreshContactSummaries();
        syncContactList();
        if (_currentConversation >= 0 && _currentConversation < _conversations.size()) {
            bindConversation(_currentConversation);
        }
    }
}

void ChatPage::onServerClosed()
{
    markAllSendingMessagesFailed();
}

QString ChatPage::createClientMessageId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString ChatPage::normalizeContactPreview(const QString &text) const
{
    const QString trimmed = text.trimmed();
    const QString lower = trimmed.toLower();
    if (lower.endsWith(".png") || lower.endsWith(".jpg") || lower.endsWith(".jpeg")
        || lower.endsWith(".bmp") || lower.endsWith(".webp") || lower.endsWith(".gif")
        || lower.contains("/uploads/chat_images/") || lower.contains("\\uploads\\chat_images\\")
        || trimmed == QStringLiteral("[image]")
        || trimmed == QStringLiteral("[IMAGE]")
        || trimmed.contains(QString::fromUtf8(u8"\u56fe\u7247"))
        || trimmed.contains(QStringLiteral("鍥剧墖"))) {
        return QString::fromUtf8(u8"[\u56fe\u7247]");
    }
    return trimmed;
}

QString ChatPage::normalizeImageResourceKey(const QString &text) const
{
    QString normalized = text.trimmed();
    if (normalized.isEmpty()) {
        return {};
    }

    normalized.replace('\\', '/');
    const QString marker = QStringLiteral("/uploads/chat_images/");
    const int markerPos = normalized.indexOf(marker, 0, Qt::CaseInsensitive);
    if (markerPos >= 0) {
        const QString suffix = normalized.mid(markerPos + marker.size());
        if (!suffix.isEmpty()) {
            return QStringLiteral("chat_images/") + QFileInfo(suffix).fileName();
        }
    }

    if (normalized.startsWith(QStringLiteral("chat_images/"), Qt::CaseInsensitive)) {
        return QStringLiteral("chat_images/") + QFileInfo(normalized.mid(QStringLiteral("chat_images/").size())).fileName();
    }

    return normalized;
}

QString ChatPage::imageCacheDirectory() const
{
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(baseDir).filePath(QStringLiteral("image_cache"));
}

QString ChatPage::localImageCachePath(const QString &resourceKey) const
{
    const QString normalized = normalizeImageResourceKey(resourceKey);
    if (normalized.isEmpty()) {
        return {};
    }

    if (QFileInfo::exists(normalized)) {
        return normalized;
    }

    if (!normalized.startsWith(QStringLiteral("chat_images/"), Qt::CaseInsensitive)) {
        return {};
    }

    const QString fileName = QFileInfo(normalized.mid(QStringLiteral("chat_images/").size())).fileName();
    if (fileName.isEmpty()) {
        return {};
    }

    QDir dir(imageCacheDirectory());
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    return dir.filePath(fileName);
}

void ChatPage::ensureImageAvailable(MessageItem &item)
{
    if (item.type != ChatMessageType::Image || item.text.isEmpty()) {
        return;
    }

    const QString localPath = localImageCachePath(item.text);
    if (!localPath.isEmpty()) {
        const QImage localImage(localPath);
        if (!localImage.isNull()) {
            item.image = localImage;
            return;
        }
    }

    const QString resourceKey = normalizeImageResourceKey(item.text);
    if (!resourceKey.startsWith(QStringLiteral("chat_images/"), Qt::CaseInsensitive)) {
        return;
    }

    requestImageDownload(resourceKey);
}

void ChatPage::cachePendingImage(const QString &clientMsgId, const QString &resourceKey)
{
    if (clientMsgId.isEmpty() || resourceKey.isEmpty()) {
        return;
    }

    const QString cachePath = localImageCachePath(resourceKey);
    if (cachePath.isEmpty()) {
        return;
    }

    for (const Conversation &conversation : std::as_const(_conversations)) {
        for (const MessageItem &message : conversation.messages) {
            if (message.clientMsgId != clientMsgId || message.type != ChatMessageType::Image || message.image.isNull()) {
                continue;
            }

            QDir cacheDir(QFileInfo(cachePath).absolutePath());
            if (!cacheDir.exists()) {
                cacheDir.mkpath(QStringLiteral("."));
            }
            message.image.save(cachePath, "PNG");
            return;
        }
    }
}

void ChatPage::requestImageDownload(const QString &resourceKey)
{
    const QString normalized = normalizeImageResourceKey(resourceKey);
    if (!normalized.startsWith(QStringLiteral("chat_images/"), Qt::CaseInsensitive)) {
        return;
    }

    const QString cachePath = localImageCachePath(normalized);
    if (!cachePath.isEmpty() && QFileInfo::exists(cachePath)) {
        return;
    }
    if (_downloadingImageResources.contains(normalized)) {
        return;
    }

    _downloadingImageResources.insert(normalized);
    QUrl url(gate_url_prefix + "/download_image");
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("path"), normalized);
    url.setQuery(query);

    QNetworkRequest request(url);
    QNetworkReply *reply = _imageDownloadManager->get(request);
    reply->setProperty("resource_key", normalized);
}

void ChatPage::onImageDownloadFinished(QNetworkReply *reply)
{
    const QString resourceKey = reply->property("resource_key").toString();
    _downloadingImageResources.remove(resourceKey);

    if (reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        return;
    }

    const QByteArray bytes = reply->readAll();
    reply->deleteLater();
    if (bytes.isEmpty()) {
        return;
    }

    const QString cachePath = localImageCachePath(resourceKey);
    if (cachePath.isEmpty()) {
        return;
    }

    QDir cacheDir(QFileInfo(cachePath).absolutePath());
    if (!cacheDir.exists() && !cacheDir.mkpath(QStringLiteral("."))) {
        return;
    }

    QFile output(cachePath);
    if (!output.open(QIODevice::WriteOnly)) {
        return;
    }
    output.write(bytes);
    output.close();

    refreshImageResource(resourceKey);
}

void ChatPage::refreshImageResource(const QString &resourceKey)
{
    const QString normalized = normalizeImageResourceKey(resourceKey);
    bool changed = false;
    for (Conversation &conversation : _conversations) {
        for (MessageItem &message : conversation.messages) {
            if (message.type != ChatMessageType::Image) {
                continue;
            }
            if (normalizeImageResourceKey(message.text) != normalized) {
                continue;
            }

            const QString cachePath = localImageCachePath(normalized);
            if (cachePath.isEmpty()) {
                continue;
            }
            const QImage image(cachePath);
            if (image.isNull()) {
                continue;
            }
            message.image = image;
            changed = true;
        }
    }

    if (changed && _currentConversation >= 0 && _currentConversation < _conversations.size()) {
        bindConversation(_currentConversation);
    }
}
