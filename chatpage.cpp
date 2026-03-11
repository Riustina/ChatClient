#include "chatpage.h"
#include "ui_chatpage.h"

#include "chatinputedit.h"
#include "contactlistwidget.h"
#include "messagelistwidget.h"

#include <QButtonGroup>
#include <QDateTime>
#include <QLinearGradient>
#include <QPainter>
#include <QRandomGenerator>
#include <algorithm>
#include <QStyle>

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
{
    ui->setupUi(this);

    setupUiExtensions();
    setupNavigation();
    setupMockData();
    sortConversationsByLatest();
    refreshContactSummaries();
    syncContactList();
    bindConversation(0);
}

ChatPage::~ChatPage()
{
    delete ui;
}

void ChatPage::onContactActivated(int index)
{
    bindConversation(index);
}

void ChatPage::onSendClicked()
{
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
    _chatInputEdit->setPlaceholderText("输入消息，Enter 发送，Shift+Enter 换行。");
    refreshContactSummaries();
    syncContactList();
    bindConversation(_currentConversation);
}

void ChatPage::onMockReceiveClicked()
{
    _conversations[_currentConversation].messages.push_back(createIncomingMockMessage());
    _currentConversation = moveConversationToFront(_currentConversation);
    refreshContactSummaries();
    syncContactList();
    bindConversation(_currentConversation);
}

void ChatPage::onImagePasted()
{
    if (_chatInputEdit->hasPendingImage()) {
        _chatInputEdit->setPlaceholderText("已插入图片预览，可直接退格删除。");
    }
}

void ChatPage::setupUiExtensions()
{
    ui->contactListLayout->addWidget(_contactListWidget);
    ui->messageListLayout->addWidget(_messageListWidget);
    ui->inputLayout->addWidget(_chatInputEdit);

    ui->chatNavButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    ui->friendRequestNavButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    ui->navFrame->setFixedWidth(40);
    ui->contactFrame->setFixedWidth(255);
    ui->chatNavButton->setIconSize(QSize(18, 18));
    ui->friendRequestNavButton->setIconSize(QSize(18, 18));
    ui->chatNavButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    ui->friendRequestNavButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    ui->composerFrame->setMinimumHeight(165);
    ui->composerFrame->setMaximumHeight(195);

    setStyleSheet(
        "ChatPage { background:#F4F3F9; }"
        "QFrame#navFrame { background:#F4F3F9; }"
        "QFrame#contactFrame { background:#F4F3F9; }"
        "QFrame#rightFrame { background:#F4F3F9; }"
        "QFrame#chatHeaderFrame { background:#F4F3F9; border-bottom:1px solid #e4e2eb; }"
        "QFrame#composerFrame { background:#F4F3F9; border-top:1px solid #e4e2eb; }"
        "QFrame#searchFrame { background:#EAE9EF; border-radius:17px; }"
        "QLineEdit#searchLineEdit { background:transparent; border:none; padding-left:14px; font: 10pt 'Microsoft YaHei UI'; color:#1f2937; }"
        "QToolButton#addFriendButton { background:#E0DEE8; border:none; border-radius:17px; font: 11pt 'Microsoft YaHei UI'; color:#4b5563; }"
        "QToolButton#chatNavButton, QToolButton#friendRequestNavButton { min-height:44px; border:none; border-radius:12px; color:#7b7a82; background:transparent; }"
        "QToolButton#chatNavButton:checked, QToolButton#friendRequestNavButton:checked { background:#CBCACF; color:#1f2937; }"
        "QLabel#chatTitleLabel { font: 15pt 'Microsoft YaHei UI'; color:#111827; }"
        "QToolButton#headerActionButton1, QToolButton#headerActionButton2, QToolButton#headerActionButton3 { background:#EAE9EF; border:none; border-radius:14px; padding:8px 14px; color:#334155; font: 10pt 'Microsoft YaHei UI'; }"
        "QPushButton#sendButton, QPushButton#mockReceiveButton { min-width:92px; min-height:38px; border:none; border-radius:19px; font: 10pt 'Microsoft YaHei UI'; }"
        "QPushButton#mockReceiveButton { background:#EAE9EF; color:#334155; }"
        "QPushButton#sendButton { background:#CBCACF; color:#111827; }"
        "QTextEdit { background:#ffffff; border:1px solid #dfdde7; border-radius:18px; padding:12px; font: 10pt 'Microsoft YaHei UI'; }"
        "QLabel#friendRequestTitleLabel { font: 18pt 'Microsoft YaHei UI'; color:#111827; }"
        "QLabel#friendRequestHintLabel { font: 11pt 'Microsoft YaHei UI'; color:#64748b; }");

    _chatInputEdit->setPlaceholderText("输入消息，Enter 发送，Shift+Enter 换行。");

    connect(_contactListWidget, &ContactListWidget::contactActivated, this, &ChatPage::onContactActivated);
    connect(ui->sendButton, &QPushButton::clicked, this, &ChatPage::onSendClicked);
    connect(ui->mockReceiveButton, &QPushButton::clicked, this, &ChatPage::onMockReceiveClicked);
    connect(_chatInputEdit, &ChatInputEdit::imagePasted, this, &ChatPage::onImagePasted);
    connect(_chatInputEdit, &ChatInputEdit::sendRequested, this, &ChatPage::onSendClicked);
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
    const QStringList names = {
        QStringLiteral("产品设计组"),
        QStringLiteral("陈星野"),
        QStringLiteral("诺拉"),
        QStringLiteral("项目群"),
        QStringLiteral("周迢"),
        QStringLiteral("奥利弗")
    };

    for (int i = 0; i < names.size(); ++i) {
        Conversation conversation;
        conversation.contact.id = i + 1;
        conversation.contact.name = names[i];
        conversation.contact.avatarColor = avatarColorForIndex(i);

        MessageItem incoming1;
        incoming1.id = ++_messageIdSeed;
        incoming1.senderName = names[i];
        incoming1.outgoing = false;
        incoming1.type = ChatMessageType::Text;
        incoming1.text = i % 2 == 0
                         ? QStringLiteral("我先发你一版新的主界面稿，帮我看看信息密度和视觉节奏是否合适。")
                         : QStringLiteral("今晚我会继续补测试数据，先把主界面的骨架走通。");
        incoming1.avatarColor = avatarColorForIndex(i);
        incoming1.timestamp = QDateTime::currentDateTime().addSecs(-(3600 * (i + 2)));

        MessageItem outgoing1;
        outgoing1.id = ++_messageIdSeed;
        outgoing1.senderName = QStringLiteral("我");
        outgoing1.outgoing = true;
        outgoing1.type = ChatMessageType::Text;
        outgoing1.text = QStringLiteral("可以，先按这个方向推进，后面再补细节。");
        outgoing1.avatarColor = QColor("#111827");
        outgoing1.timestamp = incoming1.timestamp.addSecs(900);

        MessageItem incoming2;
        incoming2.id = ++_messageIdSeed;
        incoming2.senderName = names[i];
        incoming2.outgoing = false;
        incoming2.type = (i % 3 == 0) ? ChatMessageType::Image : ChatMessageType::Text;
        incoming2.text = QStringLiteral("我再补一张参考图给你。");
        incoming2.image = buildMockImage(avatarColorForIndex(i), names[i]);
        incoming2.avatarColor = avatarColorForIndex(i);
        incoming2.timestamp = outgoing1.timestamp.addSecs(1200);

        conversation.messages = { incoming1, outgoing1, incoming2 };
        _conversations.push_back(conversation);
    }
}

void ChatPage::bindConversation(int index)
{
    if (index < 0 || index >= _conversations.size()) {
        return;
    }

    _currentConversation = index;
    ui->chatTitleLabel->setText(_conversations[index].contact.name);
    _messageListWidget->setMessages(_conversations[index].messages);
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
    message.senderName = QStringLiteral("我");
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
    message.senderName = QStringLiteral("我");
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
