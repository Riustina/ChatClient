# ChatClient 前后端对接说明

本文档只关注当前前端聊天模块在接入后端时，哪些函数/信号需要重点关注，以及建议的数据格式。

## 1. 目前核心文件

- [chatpage.h](/D:/Workspace/Graduate/ChatClient/chatpage.h)
- [chatpage.cpp](/D:/Workspace/Graduate/ChatClient/chatpage.cpp)
- [chattypes.h](/D:/Workspace/Graduate/ChatClient/chattypes.h)
- [contactlistwidget.h](/D:/Workspace/Graduate/ChatClient/contactlistwidget.h)
- [contactlistwidget.cpp](/D:/Workspace/Graduate/ChatClient/contactlistwidget.cpp)
- [messagelistwidget.h](/D:/Workspace/Graduate/ChatClient/messagelistwidget.h)
- [messagelistwidget.cpp](/D:/Workspace/Graduate/ChatClient/messagelistwidget.cpp)
- [searchpopupwidget.h](/D:/Workspace/Graduate/ChatClient/searchpopupwidget.h)
- [searchpopupwidget.cpp](/D:/Workspace/Graduate/ChatClient/searchpopupwidget.cpp)

## 2. 当前前端的数据结构

当前前端通用结构定义在 [chattypes.h](/D:/Workspace/Graduate/ChatClient/chattypes.h)。

### 联系人

```cpp
struct ContactItem {
    int id = 0;
    QString name;
    QString lastMessage;
    QString timeText;
    QColor avatarColor;
};
```

说明：

- `id`：联系人唯一标识，后端接入后必须稳定
- `name`：联系人显示名称
- `lastMessage`：联系人列表中展示的最后一条消息摘要
- `timeText`：联系人列表右上角时间文本，当前是前端格式化后的字符串
- `avatarColor`：当前是前端 mock 用的颜色，后端接入后建议改成头像 URL 或头像资源 id

### 消息

```cpp
enum class ChatMessageType {
    Text,
    Image
};

struct MessageItem {
    int id = 0;
    QString senderName;
    bool outgoing = false;
    ChatMessageType type = ChatMessageType::Text;
    QString text;
    QImage image;
    QColor avatarColor;
    QDateTime timestamp;
};
```

说明：

- `id`：消息唯一标识
- `senderName`：发送者显示名
- `outgoing`：`true` 表示自己发出，`false` 表示对方发出
- `type`：当前支持文本和图片
- `text`：文本消息内容
- `image`：图片消息内容，当前直接存 `QImage`
- `timestamp`：消息时间

### 好友申请

```cpp
enum class FriendRequestDirection {
    Outgoing,
    Incoming
};

enum class FriendRequestState {
    Pending,
    Added
};

struct FriendRequestItem {
    int id = 0;
    int contactId = 0;
    QString name;
    QColor avatarColor;
    FriendRequestDirection direction = FriendRequestDirection::Outgoing;
    FriendRequestState state = FriendRequestState::Pending;
    QDateTime createdAt;
};
```

说明：

- `direction`：`Outgoing` 表示我发出的申请，`Incoming` 表示别人发给我的申请
- `state`：当前只有 `Pending` 和 `Added`
- `contactId`：如果已经成为联系人，建议后端返回稳定联系人 id

## 3. 需要接后端的主要入口

### 3.1 初始化联系人列表

当前前端初始化数据来自：

- `ChatPage::setupMockData()`

后端接入后，这个函数应该逐步被“拉取会话列表接口”替代。  
拿到会话列表后，最终需要更新：

- `_conversations`
- `refreshContactSummaries()`
- `sortConversationsByLatest()`
- `syncContactList()`
- `bindConversation(_currentConversation)`

建议后端返回格式：

```json
[
  {
    "contact_id": 1001,
    "name": "张三",
    "avatar_url": "https://...",
    "last_message": {
      "id": 90001,
      "type": "text",
      "content": "你好",
      "timestamp": "2026-03-12T10:30:00+08:00",
      "sender_id": 1001
    }
  }
]
```

### 3.2 点击联系人切换聊天

当前联系人点击信号来源：

- `ContactListWidget::contactActivated(int index)`

前端处理入口：

- `ChatPage::onContactActivated(int index)`

这个函数当前会：

- 切第一列到“聊天”
- 切第三列到聊天页
- 绑定对应会话

后端如果需要“按联系人拉取最近消息”，可以在这个函数里接入请求。

建议流程：

1. 用户点击联系人
2. 前端拿到 `contactId`
3. 调用“拉取某联系人消息记录接口”
4. 返回后更新对应 `Conversation.messages`
5. 调用 `bindConversation(index)`

### 3.3 发送文本消息

当前发送入口：

- `ChatPage::onSendClicked()`

当前数据来源：

- `_chatInputEdit->plainTextForSend()`
- `_chatInputEdit->takePastedImage()`

这个函数现在会直接把消息插入本地 `_conversations[_currentConversation].messages`。  
后端接入后，建议改成：

1. 读取输入框内容
2. 组装发送请求
3. 调用发送消息接口
4. 成功后插入本地消息列表
5. 调用：
   - `refreshContactSummaries()`
   - `syncContactList()`
   - `bindConversation(_currentConversation)`

建议发送文本格式：

```json
{
  "conversation_id": 1001,
  "type": "text",
  "content": "你好",
  "client_msg_id": "local-uuid"
}
```

### 3.4 接收消息

当前 mock 入口：

- `ChatPage::onMockReceiveClicked()`

后端接入后，这里通常不会再依赖按钮，而是由：

- WebSocket
- TCP 长连接
- 或轮询接口

来推送消息。

收到后端新消息后，前端建议调用逻辑：

1. 找到对应 `contactId` 或 `conversationId`
2. 将新消息插入对应 `Conversation.messages`
3. 如果不是当前会话，也更新其 `lastMessage`
4. 调用：
   - `refreshContactSummaries()`
   - `moveConversationToFront(index)` 或等价排序逻辑
   - `syncContactList()`
   - 如果当前就是该会话，再 `bindConversation(index)`

建议接收消息格式：

```json
{
  "message_id": 90002,
  "conversation_id": 1001,
  "sender_id": 1001,
  "sender_name": "张三",
  "type": "text",
  "content": "收到吗",
  "timestamp": "2026-03-12T10:31:00+08:00"
}
```

### 3.5 搜索联系人 / 添加好友

搜索框当前入口：

- `ChatPage::onSearchTextChanged(const QString &text)`

当前搜索逻辑：

- `ChatPage::filteredContacts(const QString &text) const`

这部分现在完全是本地过滤。  
后端接入后可替换成：

1. 用户输入关键字
2. 前端请求“搜索用户 / 搜索联系人接口”
3. 返回结果后调用：
   - `_searchPopup->setSearchText(text)`
   - `_searchPopup->setResults(...)`

点击“添加好友”的入口：

- `ChatPage::onPopupAddFriendClicked(const QString &text)`

当前行为：

- 弹确认框
- 确认后调用 `addOutgoingFriendRequest(name)`

后端接入后，建议在确认后先调用“发送好友申请接口”，成功后再插入本地申请列表。

建议好友申请发送格式：

```json
{
  "target_keyword": "张三",
  "remark": ""
}
```

如果后端已经能明确目标用户，建议直接传：

```json
{
  "target_user_id": 1001,
  "remark": ""
}
```

### 3.6 好友申请列表

当前主动添加好友后写入入口：

- `ChatPage::addOutgoingFriendRequest(const QString &name)`

当前模拟收到好友申请入口：

- `ChatPage::onMockFriendRequestClicked()`
- `ChatPage::addIncomingFriendRequest(const QString &name)`

当前同意好友申请入口：

- `ChatPage::onFriendRequestAccepted(int requestId)`

当前这个函数会：

- 把状态改成 `Added`
- 调用 `ensureConversationForFriend(...)`
- 刷新联系人列表

后端接入后建议：

1. 页面初始化时请求好友申请列表
2. 用返回值填充 `_friendRequests`
3. 调用 `refreshFriendRequestList()`

同意好友申请时建议流程：

1. 点击申请项里的“同意”
2. 调用“同意好友申请接口”
3. 成功后更新这条 `FriendRequestItem.state = Added`
4. 若后端已返回联系人信息，则创建或更新 `Conversation`
5. 调用：
   - `refreshFriendRequestList()`
   - `syncContactList()`

建议好友申请列表格式：

```json
[
  {
    "request_id": 7001,
    "contact_id": 1005,
    "name": "李四",
    "direction": "incoming",
    "state": "pending",
    "created_at": "2026-03-12T10:35:00+08:00"
  }
]
```

建议同意好友申请请求格式：

```json
{
  "request_id": 7001,
  "action": "accept"
}
```

## 4. 当前前端内部刷新函数

下面这些函数不是后端接口，但接入后端时会经常用到：

- `refreshContactSummaries()`
  - 根据每个会话的最后一条消息，刷新联系人列表摘要和时间
- `sortConversationsByLatest()`
  - 按最新消息时间排序
- `moveConversationToFront(int index)`
  - 某个会话有新消息时移到顶部
- `syncContactList()`
  - 把 `_conversations` 同步到第二列联系人 UI
- `bindConversation(int index)`
  - 把某个会话绑定到第三列消息区域
- `refreshFriendRequestList()`
  - 把 `_friendRequests` 同步到好友申请页 UI

## 5. 建议的对接顺序

建议按下面顺序接后端，改动最稳：

1. 先接“会话列表加载”
2. 再接“发送文本消息”
3. 再接“实时接收消息”
4. 再接“搜索用户 / 添加好友”
5. 最后接“好友申请列表 / 同意好友申请”

## 6. 当前最值得注意的点

- 前端现在大量使用本地 `QVector` 作为临时数据源，后端接入后应逐步替换为接口返回数据
- 联系人选中状态依赖 `contactId` 稳定，不要让后端返回的联系人 id 每次变化
- 图片消息当前直接存 `QImage`，真实接入时更适合改成“本地文件路径 / 缩略图路径 / 远程 URL”
- `ContactItem.avatarColor` 目前只是 mock 占位，真实项目最好换成头像地址
- 搜索弹层、好友申请页、聊天页现在都已能独立前端跑通，后端接入时优先替换数据来源，不要先重写 UI
