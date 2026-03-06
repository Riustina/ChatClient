// UserMgr.h

#ifndef USERMGR_H
#define USERMGR_H

#include <QObject>
#include <QString>
#include "Singleton.h"

class UserMgr : public QObject,
                public Singleton<UserMgr>,
                public std::enable_shared_from_this<UserMgr>
{
    Q_OBJECT
    friend class Singleton<UserMgr>;

public:
    ~UserMgr() = default;

    // Setter
    void SetUid(int uid);
    void SetName(const QString& name);
    void SetToken(const QString& token);

    // Getter
    int     GetUid()    const;
    QString GetName()   const;
    QString GetToken()  const;

private:
    UserMgr();

    int     _uid   = 0;
    QString _name;
    QString _token;
};

#endif // USERMGR_H
