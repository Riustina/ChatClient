#include "chatpage.h"
#include "ui_chatpage.h"

#include "addfrienddialog.h"
#include "chatinputedit.h"
#include "contactlistwidget.h"
#include "friendrequestitemwidget.h"
#include "messagelistwidget.h"
#include "searchpopupwidget.h"

#include <QApplication>
#include <QButtonGroup>
#include <QDateTime>
#include <QEvent>
#include <QFocusEvent>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QTimer>
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

QImage buildMockImage(const QColor &baseColor, const QString &text)
{
    QImage image(320, 180, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QLinearGradient gradient(0, 0, image.width(), image.height());
    gradient.setColorAt(0.0, baseColor.lighter(130));
    gradient.setColorAt(1.0, baseColor.darker(110));
    painter.setBrush(gradient);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(image.rect(), 22, 22);

    painter.setPen(Qt::white);
    painter.setFont(QFont("Microsoft YaHei UI", 22, QFont::DemiBold));
    painter.drawText(image.rect().adjusted(22, 22, -22, -22), Qt::AlignCenter, text);
    return image;
}
}

ChatPage::ChatPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ChatPage)
    , _contactListWidget(new ContactListWidget(this))
    , _messageListWidget(new MessageListWidget(this))
    , _chatInputEdit(new ChatInputEdit(this))
    , _searchPopup(new SearchPopupWidget(this))
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
    bindConversation(index);
    syncContactList();
}

void ChatPage::onSendClicked()
{
    if (_conversations.isEmpty() || _currentConversation < 0 || _currentConversation >= _conversations.size()) {
        return;
    }

    const QImage pastedImage = _chatInputEdit->takePastedImage();
    const QString text = _chatInputEdit->plainTextForSend().trimmed();
    if (text.isEmpty() && pastedImage.isNull()) {
        return;
    }

    if (!pastedImage.isNull()) {
        _conversations[_currentConversation].messages.push_back(createOutgoingImageMessage(pastedImage));
    }
    if (!text.isEmpty()) {
        _conversations[_currentConversation].messages.push_back(createOutgoingTextMessage(text));
    }

    _chatInputEdit->clear();
    _currentConversation = moveConversationToFront(_currentConversation);
    _chatInputEdit->setPlaceholderText(QStringLiteral("输入消息，Enter 发送，Shift+Enter 换行。"));
    refreshContactSummaries();
    syncContactList();
    bindConversation(_currentConversation);
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
        _chatInputEdit->setPlaceholderText(QStringLiteral("已插入图片预览，可直接退格删除。"));
    }
}

void ChatPage::onSearchTextChanged(const QString &text)
{
    Q_UNUSED(text);
    updateSearchPopup();
}

void ChatPage::onPopupAddFriendClicked(const QString &text)
{
    hideSearchPopup();
    ui->searchLineEdit->clearFocus();

    const QString targetName = text.trimmed();
    QPointer<ChatPage> that(this);
    QTimer::singleShot(0, this, [that, targetName]() {
        if (!that) {
            return;
        }
        AddFriendDialog dialog(targetName, that);
        if (dialog.exec() == QDialog::Accepted) {
            that->addOutgoingFriendRequest(targetName);
        }
    });
}

void ChatPage::onPopupContactClicked(int contactId)
{
    const int index = conversationIndexById(contactId);
    if (index >= 0) {
        bindConversation(index);
        syncContactList();
    }
    hideSearchPopup();
}

void ChatPage::setupUiExtensions()
{
    ui->contactListLayout->addWidget(_contactListWidget);
    ui->messageListLayout->addWidget(_messageListWidget);
    ui->inputLayout->addWidget(_chatInputEdit);

    ui->chatNavButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    ui->friendRequestNavButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
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
}

void ChatPage::setupFriendRequestPage()
{
    ui->friendRequestHintLabel->setText(QStringLiteral("这里展示你发起的好友申请，以及别人向你发来的好友申请。"));

    _mockFriendRequestButton = new QPushButton(QStringLiteral("模拟收到申请"), this);
    _mockFriendRequestButton->setObjectName("mockFriendRequestButton");

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

    _currentConversation = index;
    ui->chatTitleLabel->setText(_conversations[index].contact.name);
    _messageListWidget->setMessages(_conversations[index].messages);
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
    QVector<ContactItem> results;
    if (text.isEmpty()) {
        return results;
    }
    for (const Conversation &conversation : _conversations) {
        if (conversation.contact.name.contains(text, Qt::CaseInsensitive)) {
            results.push_back(conversation.contact);
        }
    }
    return results;
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
            conversation.contact.lastMessage.clear();
            conversation.contact.timeText.clear();
            continue;
        }

        const MessageItem &last = conversation.messages.back();
        conversation.contact.lastMessage = formatMessagePreview(last);
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
    message.id = ++_messageIdSeed;
    message.senderName = _currentUserName.isEmpty() ? QStringLiteral("我") : _currentUserName;
    message.outgoing = true;
    message.type = ChatMessageType::Text;
    message.text = text;
    message.avatarColor = QColor("#111827");
    message.timestamp = QDateTime::currentDateTime();
    return message;
}

MessageItem ChatPage::createOutgoingImageMessage(const QImage &image)
{
    MessageItem message;
    message.id = ++_messageIdSeed;
    message.senderName = _currentUserName.isEmpty() ? QStringLiteral("我") : _currentUserName;
    message.outgoing = true;
    message.type = ChatMessageType::Image;
    message.image = image;
    message.avatarColor = QColor("#111827");
    message.timestamp = QDateTime::currentDateTime();
    return message;
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
        return QStringLiteral("[图片]");
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
    const int currentContactId = (_currentConversation >= 0 && _currentConversation < _conversations.size())
        ? _conversations[_currentConversation].contact.id
        : -1;

    for (FriendRequestItem &request : _friendRequests) {
        if (request.id != requestId) {
            continue;
        }

        request.state = FriendRequestState::Added;
        ensureConversationForFriend(request);
        break;
    }

    refreshContactSummaries();
    sortConversationsByLatest();
    restoreCurrentConversation(currentContactId);
    syncContactList();
    bindConversation(_currentConversation);
    refreshFriendRequestList();
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
        _friendRequestListLayout->addWidget(itemWidget);
    }

    _friendRequestListLayout->addStretch();
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
