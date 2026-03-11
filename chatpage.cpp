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
    const QString text = _chatInputEdit->toPlainText().trimmed();
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
    _chatInputEdit->setPlaceholderText("Type a message here. Paste images is supported.");
    refreshContactSummaries();
    syncContactList();
    bindConversation(_currentConversation);
}

void ChatPage::onMockReceiveClicked()
{
    _conversations[_currentConversation].messages.push_back(createIncomingMockMessage());
    refreshContactSummaries();
    syncContactList();
    bindConversation(_currentConversation);
}

void ChatPage::onImagePasted()
{
    if (_chatInputEdit->hasPendingImage()) {
        _chatInputEdit->setPlaceholderText("Image pasted. Click Send to deliver it, or keep typing.");
    }
}

void ChatPage::setupUiExtensions()
{
    ui->contactListLayout->addWidget(_contactListWidget);
    ui->messageListLayout->addWidget(_messageListWidget);
    ui->inputLayout->addWidget(_chatInputEdit);

    ui->chatNavButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    ui->friendRequestNavButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    ui->chatNavButton->setIconSize(QSize(24, 24));
    ui->friendRequestNavButton->setIconSize(QSize(24, 24));
    ui->chatNavButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    ui->friendRequestNavButton->setToolButtonStyle(Qt::ToolButtonIconOnly);

    setStyleSheet(
        "ChatPage { background:#f3f6fb; }"
        "QFrame#navFrame { background:#111827; }"
        "QFrame#contactFrame { background:#fbfdff; border-right:1px solid #e5e7eb; }"
        "QFrame#rightFrame { background:#eef3f9; }"
        "QFrame#chatHeaderFrame { background:#f8fbff; border-bottom:1px solid #dde6f1; }"
        "QFrame#composerFrame { background:#f8fbff; border-top:1px solid #dde6f1; }"
        "QFrame#searchFrame { background:#edf2f7; border-radius:20px; }"
        "QLineEdit#searchLineEdit { background:transparent; border:none; padding-left:14px; font: 10pt 'Microsoft YaHei UI'; color:#1f2937; }"
        "QToolButton#addFriendButton { background:#dbeafe; border:none; border-radius:20px; font: 12pt 'Microsoft YaHei UI'; color:#1d4ed8; }"
        "QToolButton#chatNavButton, QToolButton#friendRequestNavButton { min-height:56px; border:none; border-radius:18px; color:#9fb0c8; background:transparent; }"
        "QToolButton#chatNavButton:checked, QToolButton#friendRequestNavButton:checked { background:#1f2937; color:#ffffff; }"
        "QLabel#chatTitleLabel { font: 15pt 'Microsoft YaHei UI'; color:#111827; }"
        "QToolButton#headerActionButton1, QToolButton#headerActionButton2, QToolButton#headerActionButton3 { background:#eef2ff; border:none; border-radius:14px; padding:8px 14px; color:#334155; font: 10pt 'Microsoft YaHei UI'; }"
        "QPushButton#sendButton, QPushButton#mockReceiveButton { min-width:92px; min-height:38px; border:none; border-radius:19px; font: 10pt 'Microsoft YaHei UI'; }"
        "QPushButton#mockReceiveButton { background:#e2e8f0; color:#334155; }"
        "QPushButton#sendButton { background:#2563eb; color:white; }"
        "QTextEdit { background:#ffffff; border:1px solid #dbe3ef; border-radius:18px; padding:14px; font: 10pt 'Microsoft YaHei UI'; }"
        "QLabel#friendRequestTitleLabel { font: 18pt 'Microsoft YaHei UI'; color:#111827; }"
        "QLabel#friendRequestHintLabel { font: 11pt 'Microsoft YaHei UI'; color:#64748b; }");

    _chatInputEdit->setPlaceholderText("Type a message here. Paste images is supported.");

    connect(_contactListWidget, &ContactListWidget::contactActivated, this, &ChatPage::onContactActivated);
    connect(ui->sendButton, &QPushButton::clicked, this, &ChatPage::onSendClicked);
    connect(ui->mockReceiveButton, &QPushButton::clicked, this, &ChatPage::onMockReceiveClicked);
    connect(_chatInputEdit, &ChatInputEdit::imagePasted, this, &ChatPage::onImagePasted);
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
        "Asteria Design",
        "Chen Xingye",
        "Nora",
        "Project Group",
        "Zhou Tiao",
        "Oliver"
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
                         ? QStringLiteral("I have a new draft of the main layout. Please check the rhythm and density.")
                         : QStringLiteral("I will add more mock data tonight. For now the main shell is the priority.");
        incoming1.avatarColor = avatarColorForIndex(i);
        incoming1.timestamp = QDateTime::currentDateTime().addSecs(-(3600 * (i + 2)));

        MessageItem outgoing1;
        outgoing1.id = ++_messageIdSeed;
        outgoing1.senderName = QStringLiteral("Me");
        outgoing1.outgoing = true;
        outgoing1.type = ChatMessageType::Text;
        outgoing1.text = QStringLiteral("Works for me. Let's push this direction first and refine details later.");
        outgoing1.avatarColor = QColor("#111827");
        outgoing1.timestamp = incoming1.timestamp.addSecs(900);

        MessageItem incoming2;
        incoming2.id = ++_messageIdSeed;
        incoming2.senderName = names[i];
        incoming2.outgoing = false;
        incoming2.type = (i % 3 == 0) ? ChatMessageType::Image : ChatMessageType::Text;
        incoming2.text = QStringLiteral("Adding one more reference image here.");
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
    _contactListWidget->setContacts(contacts);
}

MessageItem ChatPage::createOutgoingTextMessage(const QString &text)
{
    MessageItem message;
    message.id = ++_messageIdSeed;
    message.senderName = QStringLiteral("Me");
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
    message.senderName = QStringLiteral("Me");
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
        QStringLiteral("I checked it. The list scrolling and layout direction are both correct."),
        QStringLiteral("Add a search popup entry next and we can continue from there."),
        QStringLiteral("I am sending a new round of mock data. Please check the bubble spacing.")
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
        return QStringLiteral("[Image]");
    }
    return message.text;
}

QString ChatPage::formatMessageTime(const QDateTime &timestamp) const
{
    return timestamp.time().toString("HH:mm");
}
