#include "localdb.h"

#include <QDebug>
#include <QDir>
#include <QMetaType>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>

namespace {
const char *kConnectionName = "chatclient_localdb";
}

LocalDb &LocalDb::instance()
{
    static LocalDb db;
    return db;
}

bool LocalDb::init()
{
    const QString dataDir = dataDirectory();
    QDir dir(dataDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        _lastError = QStringLiteral("无法创建本地数据库目录：%1").arg(dataDir);
        qWarning() << "[LocalDb] init" << _lastError;
        return false;
    }

    _dataDirectory = dataDir;
    _databasePath.clear();
    _currentUid = 0;
    _lastError.clear();
    qDebug() << "[LocalDb] 本地数据库目录已就绪:" << QDir::toNativeSeparators(_dataDirectory);
    return true;
}

bool LocalDb::switchUser(int uid)
{
    if (uid <= 0) {
        _lastError = QStringLiteral("无效的用户 uid：%1").arg(uid);
        return false;
    }

    if (_dataDirectory.isEmpty() && !init()) {
        return false;
    }

    const QString newPath = QDir(_dataDirectory).filePath(QStringLiteral("chatclient_%1.db").arg(uid));
    if (_currentUid == uid && QSqlDatabase::contains(kConnectionName)) {
        QSqlDatabase db = QSqlDatabase::database(kConnectionName);
        if (db.isOpen() && db.databaseName() == newPath) {
            _databasePath = newPath;
            _lastError.clear();
            return true;
        }
    }

    close();
    if (!openDatabase(newPath)) {
        return false;
    }

    _currentUid = uid;
    qDebug() << "[LocalDb] 已切换到用户数据库:" << uid << QDir::toNativeSeparators(_databasePath);
    return true;
}

void LocalDb::close()
{
    if (!QSqlDatabase::contains(kConnectionName)) {
        return;
    }

    {
        QSqlDatabase db = QSqlDatabase::database(kConnectionName);
        if (db.isOpen()) {
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(kConnectionName);
    _databasePath.clear();
    _currentUid = 0;
}

QString LocalDb::databasePath() const
{
    return _databasePath;
}

QString LocalDb::lastError() const
{
    return _lastError;
}

QString LocalDb::dataDirectory() const
{
    QString dataDir = _dataDirectory;
    if (dataDir.isEmpty()) {
        dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (dataDir.isEmpty()) {
            dataDir = QDir::toNativeSeparators(QDir::currentPath() + QDir::separator() + "data");
        }
    }
    return dataDir;
}

bool LocalDb::openDatabase(const QString &databasePath)
{
    QSqlDatabase db = QSqlDatabase::contains(kConnectionName)
        ? QSqlDatabase::database(kConnectionName)
        : QSqlDatabase::addDatabase("QSQLITE", kConnectionName);
    db.setDatabaseName(databasePath);

    if (!db.open()) {
        _lastError = db.lastError().text();
        qWarning() << "[LocalDb] 打开数据库失败:" << _lastError;
        return false;
    }

    _databasePath = databasePath;
    if (!createTables()) {
        db.close();
        _databasePath.clear();
        return false;
    }

    _lastError.clear();
    return true;
}

bool LocalDb::createTables()
{
    QSqlDatabase db = QSqlDatabase::database(kConnectionName);
    if (!db.isOpen()) {
        _lastError = QStringLiteral("本地数据库未打开");
        return false;
    }

    const QStringList sqlList = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS sync_state ("
            "key TEXT PRIMARY KEY,"
            "value TEXT NOT NULL DEFAULT '',"
            "updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS contact_summary ("
            "contact_id INTEGER PRIMARY KEY,"
            "name TEXT NOT NULL DEFAULT '',"
            "last_message TEXT NOT NULL DEFAULT '',"
            "last_time TEXT NOT NULL DEFAULT '',"
            "unread_count INTEGER NOT NULL DEFAULT 0,"
            "updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS private_message ("
            "msg_id INTEGER PRIMARY KEY,"
            "contact_id INTEGER NOT NULL,"
            "from_uid INTEGER NOT NULL,"
            "to_uid INTEGER NOT NULL,"
            "content_type TEXT NOT NULL DEFAULT 'text',"
            "content TEXT NOT NULL DEFAULT '',"
            "created_at TEXT NOT NULL DEFAULT '',"
            "read_at TEXT DEFAULT NULL)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS friend_request ("
            "request_id INTEGER PRIMARY KEY,"
            "from_uid INTEGER NOT NULL,"
            "to_uid INTEGER NOT NULL,"
            "name TEXT NOT NULL DEFAULT '',"
            "remark TEXT NOT NULL DEFAULT '',"
            "status TEXT NOT NULL DEFAULT 'pending',"
            "updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)")
    };

    for (const QString &sql : sqlList) {
        QSqlQuery query(db);
        if (!query.exec(sql)) {
            _lastError = query.lastError().text();
            qWarning() << "[LocalDb] 建表失败:" << _lastError << "SQL:" << sql;
            return false;
        }
    }

    return true;
}

bool LocalDb::saveFriendList(const QVector<ContactItem> &contacts)
{
    QSqlDatabase db = QSqlDatabase::database(kConnectionName);
    if (!db.isOpen()) {
        _lastError = QStringLiteral("本地数据库未打开");
        return false;
    }

    if (!db.transaction()) {
        _lastError = db.lastError().text();
        return false;
    }

    QSqlQuery clearQuery(db);
    if (!clearQuery.exec(QStringLiteral("DELETE FROM contact_summary"))) {
        _lastError = clearQuery.lastError().text();
        db.rollback();
        return false;
    }

    for (const ContactItem &contact : contacts) {
        if (!upsertContactSummaryItem(contact)) {
            db.rollback();
            return false;
        }
    }

    if (!db.commit()) {
        _lastError = db.lastError().text();
        db.rollback();
        return false;
    }

    return true;
}

bool LocalDb::saveFriendRequests(const QVector<FriendRequestItem> &requests, int currentUserId)
{
    QSqlDatabase db = QSqlDatabase::database(kConnectionName);
    if (!db.isOpen()) {
        _lastError = QStringLiteral("本地数据库未打开");
        return false;
    }

    if (!db.transaction()) {
        _lastError = db.lastError().text();
        return false;
    }

    QSqlQuery clearQuery(db);
    if (!clearQuery.exec(QStringLiteral("DELETE FROM friend_request"))) {
        _lastError = clearQuery.lastError().text();
        db.rollback();
        return false;
    }

    for (const FriendRequestItem &request : requests) {
        if (!upsertFriendRequestItem(request, currentUserId)) {
            db.rollback();
            return false;
        }
    }

    if (!db.commit()) {
        _lastError = db.lastError().text();
        db.rollback();
        return false;
    }

    return true;
}

bool LocalDb::setSyncValue(const QString &key, const QString &value)
{
    QSqlDatabase db = QSqlDatabase::database(kConnectionName);
    if (!db.isOpen()) {
        _lastError = QStringLiteral("本地数据库未打开");
        return false;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO sync_state(key, value, updated_at) "
        "VALUES(?, ?, CURRENT_TIMESTAMP)"));
    query.addBindValue(key);
    query.addBindValue(value);
    if (!query.exec()) {
        _lastError = query.lastError().text();
        qWarning() << "[LocalDb] 保存 sync_state 失败:" << _lastError;
        return false;
    }
    return true;
}

QString LocalDb::syncValue(const QString &key, const QString &defaultValue)
{
    QSqlDatabase db = QSqlDatabase::database(kConnectionName);
    if (!db.isOpen()) {
        _lastError = QStringLiteral("本地数据库未打开");
        return defaultValue;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT value FROM sync_state WHERE key = ? LIMIT 1"));
    query.addBindValue(key);
    if (!query.exec()) {
        _lastError = query.lastError().text();
        qWarning() << "[LocalDb] 查询 sync_state 失败:" << _lastError;
        return defaultValue;
    }

    if (!query.next()) {
        return defaultValue;
    }
    return query.value(0).toString();
}

qint64 LocalDb::conversationCursor(int contactId) const
{
    auto *self = const_cast<LocalDb *>(this);
    bool ok = false;
    const qint64 cursor = self->syncValue(QStringLiteral("private_message_cursor_%1").arg(contactId), QStringLiteral("0")).toLongLong(&ok);
    return ok ? cursor : 0;
}

bool LocalDb::setConversationCursor(int contactId, qint64 msgId)
{
    return setSyncValue(QStringLiteral("private_message_cursor_%1").arg(contactId), QString::number(msgId));
}

bool LocalDb::replaceConversationMessages(int contactId, const QVector<MessageItem> &messages, int currentUserId)
{
    QSqlDatabase db = QSqlDatabase::database(kConnectionName);
    if (!db.isOpen()) {
        _lastError = QStringLiteral("本地数据库未打开");
        return false;
    }

    if (!db.transaction()) {
        _lastError = db.lastError().text();
        return false;
    }

    QSqlQuery deleteQuery(db);
    deleteQuery.prepare(QStringLiteral("DELETE FROM private_message WHERE contact_id = ?"));
    deleteQuery.addBindValue(contactId);
    if (!deleteQuery.exec()) {
        _lastError = deleteQuery.lastError().text();
        db.rollback();
        return false;
    }

    for (const MessageItem &message : messages) {
        if (!upsertMessageItem(contactId, message, currentUserId)) {
            db.rollback();
            return false;
        }
    }

    if (!db.commit()) {
        _lastError = db.lastError().text();
        db.rollback();
        return false;
    }

    qint64 maxMsgId = 0;
    for (const MessageItem &message : messages) {
        maxMsgId = qMax(maxMsgId, static_cast<qint64>(message.id));
    }
    if (maxMsgId > 0) {
        setConversationCursor(contactId, maxMsgId);
    }

    return true;
}

bool LocalDb::upsertMessage(int contactId, const MessageItem &message, int currentUserId)
{
    QSqlDatabase db = QSqlDatabase::database(kConnectionName);
    if (!db.isOpen()) {
        _lastError = QStringLiteral("本地数据库未打开");
        return false;
    }

    const bool ok = upsertMessageItem(contactId, message, currentUserId);
    if (ok && message.id > 0) {
        const qint64 currentCursor = conversationCursor(contactId);
        if (message.id > currentCursor) {
            setConversationCursor(contactId, message.id);
        }
    }
    return ok;
}

bool LocalDb::upsertContactSummaryItem(const ContactItem &contact)
{
    QSqlDatabase db = QSqlDatabase::database(kConnectionName);
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO contact_summary("
        "contact_id, name, last_message, last_time, unread_count, updated_at"
        ") VALUES(?, ?, ?, ?, ?, ?)"));
    query.addBindValue(contact.id);
    query.addBindValue(contact.name);
    query.addBindValue(contact.lastMessage);
    query.addBindValue(contact.timeText);
    query.addBindValue(contact.unreadCount);
    query.addBindValue(contact.updatedAt.isValid()
                           ? contact.updatedAt.toString(Qt::ISODate)
                           : QDateTime::currentDateTime().toString(Qt::ISODate));
    if (!query.exec()) {
        _lastError = query.lastError().text();
        qWarning() << "[LocalDb] 保存 contact_summary 失败:" << _lastError;
        return false;
    }
    return true;
}

bool LocalDb::upsertFriendRequestItem(const FriendRequestItem &request, int currentUserId)
{
    QSqlDatabase db = QSqlDatabase::database(kConnectionName);
    QSqlQuery query(db);

    QString status;
    switch (request.state) {
    case FriendRequestState::Added:
        status = QStringLiteral("accepted");
        break;
    case FriendRequestState::Rejected:
        status = QStringLiteral("rejected");
        break;
    default:
        status = QStringLiteral("pending");
        break;
    }

    const int fromUid = request.direction == FriendRequestDirection::Outgoing ? currentUserId : request.contactId;
    const int toUid = request.direction == FriendRequestDirection::Outgoing ? request.contactId : currentUserId;

    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO friend_request("
        "request_id, from_uid, to_uid, name, remark, status, updated_at"
        ") VALUES(?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP)"));
    query.addBindValue(request.id);
    query.addBindValue(fromUid);
    query.addBindValue(toUid);
    query.addBindValue(request.name);
    query.addBindValue(request.remark);
    query.addBindValue(status);
    if (!query.exec()) {
        _lastError = query.lastError().text();
        qWarning() << "[LocalDb] 保存 friend_request 失败:" << _lastError;
        return false;
    }
    return true;
}

bool LocalDb::upsertMessageItem(int contactId, const MessageItem &message, int currentUserId)
{
    QSqlDatabase db = QSqlDatabase::database(kConnectionName);
    QSqlQuery query(db);

    const int fromUid = message.outgoing ? currentUserId : contactId;
    const int toUid = message.outgoing ? contactId : currentUserId;
    const QString contentType = message.type == ChatMessageType::Image ? QStringLiteral("image") : QStringLiteral("text");
    const QString readAt = message.outgoing ? message.timestamp.toString(Qt::ISODate) : QString();

    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO private_message("
        "msg_id, contact_id, from_uid, to_uid, content_type, content, created_at, read_at"
        ") VALUES(?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(message.id);
    query.addBindValue(contactId);
    query.addBindValue(fromUid);
    query.addBindValue(toUid);
    query.addBindValue(contentType);
    query.addBindValue(message.text);
    query.addBindValue(message.timestamp.toString(Qt::ISODate));
    if (readAt.isEmpty()) {
        query.addBindValue(QVariant(QMetaType(QMetaType::QString)));
    } else {
        query.addBindValue(readAt);
    }

    if (!query.exec()) {
        _lastError = query.lastError().text();
        qWarning() << "[LocalDb] 保存 private_message 失败:" << _lastError;
        return false;
    }
    return true;
}

QVector<ContactItem> LocalDb::loadFriendList()
{
    QVector<ContactItem> contacts;
    QSqlDatabase db = QSqlDatabase::database(kConnectionName);
    if (!db.isOpen()) {
        _lastError = QStringLiteral("本地数据库未打开");
        return contacts;
    }

    QSqlQuery query(db);
    if (!query.exec(QStringLiteral(
            "SELECT contact_id, name, last_message, last_time, unread_count, updated_at "
            "FROM contact_summary ORDER BY updated_at DESC, contact_id ASC"))) {
        _lastError = query.lastError().text();
        qWarning() << "[LocalDb] 读取 contact_summary 失败:" << _lastError;
        return contacts;
    }

    while (query.next()) {
        ContactItem contact;
        contact.id = query.value(0).toInt();
        contact.name = query.value(1).toString();
        contact.lastMessage = query.value(2).toString();
        contact.timeText = query.value(3).toString();
        contact.unreadCount = query.value(4).toInt();
        contact.updatedAt = QDateTime::fromString(query.value(5).toString(), Qt::ISODate);
        contacts.push_back(contact);
    }

    return contacts;
}

QVector<FriendRequestItem> LocalDb::loadFriendRequests(int currentUserId)
{
    QVector<FriendRequestItem> requests;
    QSqlDatabase db = QSqlDatabase::database(kConnectionName);
    if (!db.isOpen()) {
        _lastError = QStringLiteral("本地数据库未打开");
        return requests;
    }

    QSqlQuery query(db);
    if (!query.exec(QStringLiteral(
            "SELECT request_id, from_uid, to_uid, name, remark, status "
            "FROM friend_request ORDER BY updated_at DESC, request_id DESC"))) {
        _lastError = query.lastError().text();
        qWarning() << "[LocalDb] 读取 friend_request 失败:" << _lastError;
        return requests;
    }

    while (query.next()) {
        FriendRequestItem item;
        item.id = query.value(0).toInt();
        const int fromUid = query.value(1).toInt();
        const int toUid = query.value(2).toInt();
        item.name = query.value(3).toString();
        item.remark = query.value(4).toString();
        const QString status = query.value(5).toString();
        item.direction = fromUid == currentUserId ? FriendRequestDirection::Outgoing : FriendRequestDirection::Incoming;
        item.contactId = item.direction == FriendRequestDirection::Outgoing ? toUid : fromUid;
        if (status == QStringLiteral("accepted")) {
            item.state = FriendRequestState::Added;
        } else if (status == QStringLiteral("rejected")) {
            item.state = FriendRequestState::Rejected;
        } else {
            item.state = FriendRequestState::Pending;
        }
        requests.push_back(item);
    }

    return requests;
}

QVector<MessageItem> LocalDb::loadConversationMessages(int contactId, int currentUserId, int limit, qint64 beforeMsgId)
{
    QVector<MessageItem> messages;
    QSqlDatabase db = QSqlDatabase::database(kConnectionName);
    if (!db.isOpen()) {
        _lastError = QStringLiteral("本地数据库未打开");
        return messages;
    }

    QSqlQuery query(db);
    if (beforeMsgId > 0 && limit > 0) {
        query.prepare(QStringLiteral(
            "SELECT * FROM ("
            "SELECT msg_id, from_uid, to_uid, content_type, content, created_at "
            "FROM private_message WHERE contact_id = ? AND msg_id < ? "
            "ORDER BY created_at DESC, msg_id DESC LIMIT ?"
            ") t ORDER BY created_at ASC, msg_id ASC"));
        query.addBindValue(contactId);
        query.addBindValue(beforeMsgId);
        query.addBindValue(limit);
    } else if (limit > 0) {
        query.prepare(QStringLiteral(
            "SELECT * FROM ("
            "SELECT msg_id, from_uid, to_uid, content_type, content, created_at "
            "FROM private_message WHERE contact_id = ? "
            "ORDER BY created_at DESC, msg_id DESC LIMIT ?"
            ") t ORDER BY created_at ASC, msg_id ASC"));
        query.addBindValue(contactId);
        query.addBindValue(limit);
    } else {
        query.prepare(QStringLiteral(
            "SELECT msg_id, from_uid, to_uid, content_type, content, created_at "
            "FROM private_message WHERE contact_id = ? ORDER BY created_at ASC, msg_id ASC"));
        query.addBindValue(contactId);
    }
    if (!query.exec()) {
        _lastError = query.lastError().text();
        qWarning() << "[LocalDb] 读取 private_message 失败:" << _lastError;
        return messages;
    }

      while (query.next()) {
          MessageItem item;
          item.id = query.value(0).toInt();
          item.outgoing = query.value(1).toInt() == currentUserId;
          item.type = query.value(3).toString() == QStringLiteral("image") ? ChatMessageType::Image : ChatMessageType::Text;
          item.text = query.value(4).toString();
          if (item.type == ChatMessageType::Image && !item.text.isEmpty()) {
              item.image = QImage(item.text);
          }
          item.timestamp = QDateTime::fromString(query.value(5).toString(), Qt::ISODate);
        if (!item.timestamp.isValid()) {
            item.timestamp = QDateTime::fromString(query.value(5).toString(), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        }
        if (!item.timestamp.isValid()) {
            item.timestamp = QDateTime::currentDateTime();
        }
        messages.push_back(item);
    }

    qint64 maxMsgId = 0;
    for (const MessageItem &item : messages) {
        maxMsgId = qMax(maxMsgId, static_cast<qint64>(item.id));
    }
    if (maxMsgId > 0 && conversationCursor(contactId) < maxMsgId) {
        setConversationCursor(contactId, maxMsgId);
    }

    return messages;
}
