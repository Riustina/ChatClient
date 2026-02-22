#include "httpmgr.h"
#include <QJsonDocument>    // 用于处理JSON数据
#include <QByteArray>       // 用于处理字节数组
#include <QNetworkReply>    // 用于处理网络回复

HttpMgr::HttpMgr() {

}

HttpMgr::~HttpMgr()
{
    connect(this, &HttpMgr::sig_http_finished, this, &HttpMgr::slot_http_finished); // 连接信号和槽
}

void HttpMgr::PostHttpReq(QUrl url, QJsonObject json, ReqId req_id, Modules module)
{
    QByteArray data = QJsonDocument(json).toJson();     // 将JSON对象转换为字节数组
    QNetworkRequest request(url);                       // 创建网络请求对象
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json"); // 设置请求头为JSON格式
    request.setHeader(QNetworkRequest::ContentLengthHeader, QByteArray::number(data.size())); // 设置请求体长度
    QNetworkReply *reply = _manager.post(request, data); // 发送POST请求

    auto self = shared_from_this();
    QObject::connect(reply, &QNetworkReply::finished, [self, reply, req_id, module]() {
        // 处理HTTP响应
        if (reply->error() == QNetworkReply::NoError) {
            QString response = reply->readAll(); // 读取响应数据
            emit self->sig_http_finished(req_id, response, ErrorCodes::SUCCESS, module); // 发出信号，通知请求完成和回包数据
        } else {
            qDebug() << "[HttpMgr.cpp] 函数 [PostHttpReq] HTTP request failed: " << reply->errorString(); // 输出错误信息
            emit self->sig_http_finished(req_id, "", ErrorCodes::ERR_NETWORK, module); // 发出信号，通知请求失败
        }
        reply->deleteLater(); // 释放网络回复对象
    });
}

void HttpMgr::slot_http_finished(ReqId req_id, QString response, ErrorCodes err, Modules module)
{
    if (module == Modules::REGISTERMOD) {
        // 发送信号通知指定模块的 HTTP 的响应结束了
        emit sig_reg_mod_http_finished(req_id, response, err);
    }
}

