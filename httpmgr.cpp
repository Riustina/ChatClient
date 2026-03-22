#include "httpmgr.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QIODevice>
#include <QNetworkReply>

HttpMgr::HttpMgr()
{
    connect(this, &HttpMgr::sig_http_finished, this, &HttpMgr::slot_http_finished);
}

HttpMgr::~HttpMgr()
{
}

void HttpMgr::PostHttpReq(QUrl url, QJsonObject json, ReqId req_id, Modules module)
{
    PostHttpReq(std::move(url), QJsonDocument(json).toJson(), req_id, module);
}

void HttpMgr::PostHttpReq(QUrl url, const QByteArray &data, ReqId req_id, Modules module)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::ContentLengthHeader, QByteArray::number(data.size()));
    QNetworkReply *reply = _manager.post(request, data);

    QObject::connect(reply, &QNetworkReply::finished, [this, reply, req_id, module]() {
        if (reply->error() == QNetworkReply::NoError) {
            const QByteArray rawResponse = reply->readAll();
            const QString response = QString::fromUtf8(rawResponse);
            emit this->sig_http_finished(req_id, response, ErrorCodes::SUCCESS, module);
        } else {
            qDebug() << "[HttpMgr.cpp] 函数 [PostHttpReq] HTTP request failed:" << reply->errorString();
            emit this->sig_http_finished(req_id, "", ErrorCodes::ERR_NETWORK, module);
        }
        reply->deleteLater();
    });
}

void HttpMgr::PostHttpReq(QUrl url, QIODevice *device, qint64 contentLength, ReqId req_id, Modules module)
{
    if (device == nullptr) {
        emit this->sig_http_finished(req_id, "", ErrorCodes::ERR_NETWORK, module);
        return;
    }

    if (!device->isOpen() && !device->open(QIODevice::ReadOnly)) {
        device->deleteLater();
        emit this->sig_http_finished(req_id, "", ErrorCodes::ERR_NETWORK, module);
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::ContentLengthHeader, QByteArray::number(contentLength));
    QNetworkReply *reply = _manager.post(request, device);
    device->setParent(reply);

    QObject::connect(reply, &QNetworkReply::finished, [this, reply, req_id, module]() {
        if (reply->error() == QNetworkReply::NoError) {
            const QByteArray rawResponse = reply->readAll();
            const QString response = QString::fromUtf8(rawResponse);
            emit this->sig_http_finished(req_id, response, ErrorCodes::SUCCESS, module);
        } else {
            qDebug() << "[HttpMgr.cpp] 函数 [PostHttpReq] HTTP request failed:" << reply->errorString();
            emit this->sig_http_finished(req_id, "", ErrorCodes::ERR_NETWORK, module);
        }
        reply->deleteLater();
    });
}

void HttpMgr::slot_http_finished(ReqId req_id, QString response, ErrorCodes err, Modules module)
{
    if (module == Modules::REGISTERMOD) {
        emit sig_reg_mod_http_finished(req_id, response, err);
    }

    if (module == Modules::RESETMOD) {
        emit sig_reset_mod_http_finished(req_id, response, err);
    }

    if (module == Modules::LOGINMOD) {
        emit sig_login_mod_http_finished(req_id, response, err);
    }

    if (module == Modules::CHATMOD) {
        emit sig_chat_mod_http_finished(req_id, response, err);
    }
}
