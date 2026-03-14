// TcpMgr.h

#ifndef TCPMGR_H
#define TCPMGR_H

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>
#include <QJsonObject>
#include "Singleton.h"
#include "global.h"

class TcpMgr : public QObject,
               public Singleton<TcpMgr>,
               public std::enable_shared_from_this<TcpMgr>
{
    Q_OBJECT

    friend class Singleton<TcpMgr>;

public:
    ~TcpMgr() = default;

public slots:
    void slot_tcp_connect(ServerInfo si);
    void slot_send_data(ReqId reqId, QString data);

signals:
    void sig_con_success(bool bSuccess);
    // 外部调用这个信号来触发发送，TcpMgr 内部把它连到 slot_send_data
    void sig_send_data(ReqId reqId, QString data);
    void sig_login_failed(int err);
    void sig_switch_chatdlg();
    void sig_server_closed();
    void sig_search_user_rsp(const QJsonObject &payload);
    void sig_add_friend_rsp(const QJsonObject &payload);
    void sig_friend_requests_rsp(const QJsonObject &payload);
    void sig_handle_friend_request_rsp(const QJsonObject &payload);
    void sig_friend_requests_push(const QJsonObject &payload);

private:
    explicit TcpMgr();

    void initHandlers();
    void dispatchMsg(quint16 msgId, const QByteArray& body);

    QTcpSocket  _socket;
    QString     _host;
    quint16     _port;

    // 粘包处理用的接收缓冲区和状态
    QByteArray  _buffer;
    bool        _b_recv_pending; // 正在等待包体剩余数据
    quint16     _message_id;
    quint16     _message_len;
    bool        _chat_logged_in;

    using MsgHandler = std::function<void(ReqId, int, QByteArray)>;
    QHash<ReqId, MsgHandler> _handlers;   // QHash O(1) 查找
};

#endif // TCPMGR_H
