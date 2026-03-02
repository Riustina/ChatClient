#ifndef HTTPMGR_H
#define HTTPMGR_H
#include <QObject>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QString>
#include <QJsonObject>      // 用于构建JSON对象
#include "Singleton.h"
#include "global.h"

class HttpMgr : public QObject,
                public Singleton<HttpMgr>,
                public std::enable_shared_from_this<HttpMgr>
{
    Q_OBJECT
public:
    ~HttpMgr();
    void PostHttpReq(QUrl url, QJsonObject json, ReqId req_id, Modules module);

private:
    friend class Singleton<HttpMgr>;
    HttpMgr();


    QNetworkAccessManager _manager;

private slots:
    void slot_http_finished(ReqId id, QString res, ErrorCodes err, Modules module);

signals:
    void sig_http_finished(ReqId id, QString res, ErrorCodes err, Modules module);
    void sig_reg_mod_http_finished(ReqId id, QString res, ErrorCodes err);
};

#endif // HTTPMGR_H
