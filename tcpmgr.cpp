// TcpMgr.cpp

#include "TcpMgr.h"
#include "UserMgr.h"
#include <QDataStream>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

TcpMgr::TcpMgr()
    : _host("")
    , _port(0)
    , _b_recv_pending(false)
    , _message_id(0)
    , _message_len(0)
    , _chat_logged_in(false)
{
    // —— 连接成功 ————————————————————————————————————————————————————————
    connect(&_socket, &QTcpSocket::connected, this, [this]() {
        qDebug() << "[TcpMgr] connected: 连接服务器成功";
        emit sig_con_success(true);
    });

    // —— 断开连接 ————————————————————————————————————————————————————————
    connect(&_socket, &QTcpSocket::disconnected, this, [this]() {
        qDebug() << "[TcpMgr] disconnected: 与服务器断开连接";
        if (_chat_logged_in) {
            _chat_logged_in = false;
            emit sig_server_closed();
        }
    });

    // —— 错误处理（Qt6 用 errorOccurred）———————————————————————————
    connect(&_socket, &QAbstractSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError err) {
                qDebug() << "[TcpMgr] errorOccurred:" << _socket.errorString();
                switch (err) {
                case QAbstractSocket::ConnectionRefusedError:
                    qDebug() << "[TcpMgr] 服务器拒绝连接";
                    emit sig_con_success(false);
                    break;
                case QAbstractSocket::HostNotFoundError:
                    qDebug() << "[TcpMgr] 找不到服务器主机";
                    emit sig_con_success(false);
                    break;
                case QAbstractSocket::SocketTimeoutError:
                    qDebug() << "[TcpMgr] 连接超时";
                    emit sig_con_success(false);
                    break;
                case QAbstractSocket::RemoteHostClosedError:
                    qDebug() << "[TcpMgr] 服务器主动关闭连接";
                    break;
                default:
                    qDebug() << "[TcpMgr] 其他网络错误，code:" << err;
                    break;
                }
            });

    // —— 收到数据：处理粘包 ———————————————————————————————————————————
    connect(&_socket, &QTcpSocket::readyRead, this, [this]() {
        _buffer.append(_socket.readAll());

        forever {
            // 1. 解析包头（msgId 2字节 + msgLen 2字节 = 4字节）
            if (!_b_recv_pending) {
                constexpr int HEADER_SIZE = static_cast<int>(sizeof(quint16) * 2);
                if (_buffer.size() < HEADER_SIZE) {
                    return; // 包头还没收完，继续等
                }

                QDataStream stream(_buffer);
                stream.setByteOrder(QDataStream::BigEndian);
                stream >> _message_id >> _message_len;

                _buffer = _buffer.mid(HEADER_SIZE);
                qDebug() << "[TcpMgr] 解析包头 -> msgId:" << _message_id
                         << "  msgLen:" << _message_len;
            }

            // 2. 等待包体
            if (_buffer.size() < static_cast<int>(_message_len)) {
                _b_recv_pending = true;
                return; // 包体还没收完，继续等
            }

            // 3. 包体已完整，取出并派发
            _b_recv_pending = false;
            QByteArray body = _buffer.left(_message_len);
            _buffer = _buffer.mid(_message_len);

            qDebug() << "[TcpMgr] 收到完整消息，msgId:" << _message_id
                     << "  body:" << body;

            dispatchMsg(_message_id, body);
            // 继续 forever 循环，处理缓冲区里可能还有的下一条消息
        }
    });

    // —— 自身 sig_send_data -> slot_send_data ————————————————————————
    // 保证跨线程调用发送时也能回到 TcpMgr 所在线程执行
    connect(this, &TcpMgr::sig_send_data, this, &TcpMgr::slot_send_data);
    initHandlers();
}

void TcpMgr::initHandlers()
{
    // 处理登录回包 ID_CHAT_LOGIN_RSP
    _handlers[ID_CHAT_LOGIN_RSP] = [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len)
        qDebug() << "[TcpMgr] initHandlers 收到登录回包，id:" << id;

        // 1. 解析 JSON
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if (jsonDoc.isNull() || !jsonDoc.isObject()) {
            qDebug() << "[TcpMgr] initHandlers JSON 解析失败";
            _chat_logged_in = false;
            emit sig_login_failed(ErrorCodes::ERR_JSON);
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();

        // 2. 检查 error 字段
        if (!jsonObj.contains("error")) {
            qDebug() << "[TcpMgr] initHandlers 回包缺少 error 字段";
            _chat_logged_in = false;
            emit sig_login_failed(ErrorCodes::ERR_JSON);
            return;
        }

        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            qDebug() << "[TcpMgr] initHandlers 登录失败，error:" << err;
            _chat_logged_in = false;
            emit sig_login_failed(err);
            return;
        }

        // 3. 写入 UserMgr
        auto& userMgr = UserMgr::getInstance();
        userMgr.SetUid(jsonObj["uid"].toInt());
        userMgr.SetName(jsonObj["name"].toString());
        userMgr.SetToken(jsonObj["token"].toString());
        _chat_logged_in = true;

        qDebug() << "[TcpMgr] initHandlers 登录成功，uid:" << userMgr.GetUid()
                 << "name:" << userMgr.GetName();

        emit sig_switch_chatdlg(); // 切换到聊天界面
    };

    auto jsonForwarder = [this](ReqId targetId, const QByteArray &data, auto signalEmitter) {
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if (jsonDoc.isNull() || !jsonDoc.isObject()) {
            qDebug() << "[TcpMgr] initHandlers JSON 解析失败, msgId:" << targetId;
            return;
        }
        signalEmitter(jsonDoc.object());
    };

    _handlers[ID_SEARCH_USER_RSP] = [this, jsonForwarder](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len)
        jsonForwarder(id, data, [this](const QJsonObject &obj) {
            emit sig_search_user_rsp(obj);
        });
    };

    _handlers[ID_ADD_FRIEND_RSP] = [this, jsonForwarder](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len)
        jsonForwarder(id, data, [this](const QJsonObject &obj) {
            emit sig_add_friend_rsp(obj);
        });
    };

    _handlers[ID_GET_FRIEND_REQUESTS_RSP] = [this, jsonForwarder](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len)
        jsonForwarder(id, data, [this](const QJsonObject &obj) {
            emit sig_friend_requests_rsp(obj);
        });
    };

    _handlers[ID_HANDLE_FRIEND_REQUEST_RSP] = [this, jsonForwarder](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len)
        jsonForwarder(id, data, [this](const QJsonObject &obj) {
            emit sig_handle_friend_request_rsp(obj);
        });
    };

    _handlers[ID_FRIEND_REQUESTS_PUSH] = [this, jsonForwarder](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len)
        jsonForwarder(id, data, [this](const QJsonObject &obj) {
            emit sig_friend_requests_push(obj);
            emit sig_friend_requests_rsp(obj);
        });
    };

    _handlers[ID_FRIEND_LIST_PUSH] = [this, jsonForwarder](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len)
        jsonForwarder(id, data, [this](const QJsonObject &obj) {
            emit sig_friend_list_push(obj);
        });
    };
}

// —————————————————————————————————————————————————————————————————————————
// 连接服务器
// —————————————————————————————————————————————————————————————————————————
void TcpMgr::slot_tcp_connect(ServerInfo si)
{
    qDebug() << "[TcpMgr] slot_tcp_connect: 正在连接"
             << si.Host << ":" << si.Port;

    _host = si.Host;
    _port = static_cast<quint16>(si.Port.toUInt());
    _socket.connectToHost(_host, _port);
}

// —————————————————————————————————————————————————————————————————————————
// 打包并发送数据
// 包格式：[msgId: 2字节][msgLen: 2字节][body: N字节]
// msgLen 是 body 的字节长度（UTF-8）
// —————————————————————————————————————————————————————————————————————————
void TcpMgr::slot_send_data(ReqId reqId, QString data)
{
    QByteArray body = data.toUtf8(); // 转 UTF-8，正确计算字节长度

    quint16 id  = static_cast<quint16>(reqId);
    quint16 len = static_cast<quint16>(body.size()); // 字节数，非字符数

    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::BigEndian);
    out << id << len;
    packet.append(body);

    qDebug() << "[TcpMgr] slot_send_data: 发送 msgId:" << id
             << "  bodyLen:" << len;

    _socket.write(packet);
}

// —————————————————————————————————————————————————————————————————————————
// 根据 msgId 派发消息（后续在这里扩展 handler）
// —————————————————————————————————————————————————————————————————————————
void TcpMgr::dispatchMsg(quint16 msgId, const QByteArray& body)
{
    ReqId id = static_cast<ReqId>(msgId);
    auto it = _handlers.find(id);
    if (it == _handlers.end()) {
        qDebug() << "[TcpMgr] dispatchMsg 未找到 msgId [" << msgId << "] 的处理函数";
        return;
    }
    it.value()(id, body.size(), body);
}
