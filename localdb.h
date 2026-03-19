#ifndef LOCALDB_H
#define LOCALDB_H

#include <QString>
#include <QVector>

#include "chattypes.h"

class LocalDb
{
public:
    static LocalDb &instance();

    bool init();
    bool switchUser(int uid);
    void close();
    QString databasePath() const;
    QString lastError() const;

    bool saveFriendList(const QVector<ContactItem> &contacts);
    bool saveFriendRequests(const QVector<FriendRequestItem> &requests, int currentUserId);
    bool replaceConversationMessages(int contactId, const QVector<MessageItem> &messages, int currentUserId);
    bool upsertMessage(int contactId, const MessageItem &message, int currentUserId);
    QVector<ContactItem> loadFriendList();
    QVector<FriendRequestItem> loadFriendRequests(int currentUserId);
    QVector<MessageItem> loadConversationMessages(int contactId, int currentUserId);

private:
    LocalDb() = default;
    LocalDb(const LocalDb &) = delete;
    LocalDb &operator=(const LocalDb &) = delete;

    bool createTables();
    bool upsertContactSummaryItem(const ContactItem &contact);
    bool upsertFriendRequestItem(const FriendRequestItem &request, int currentUserId);
    bool upsertMessageItem(int contactId, const MessageItem &message, int currentUserId);
    QString dataDirectory() const;
    bool openDatabase(const QString &databasePath);

    QString _dataDirectory;
    QString _databasePath;
    QString _lastError;
    int _currentUid = 0;
};

#endif // LOCALDB_H
