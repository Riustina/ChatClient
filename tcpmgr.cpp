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
    , _reconnect_in_progress(false)
    , _reconnect_attempted(false)
{
    connect(&_socket, &QTcpSocket::connected, this, [this]() {
        qDebug() << "[TcpMgr] connected:" << _host << _port;
        if (_reconnect_in_progress) {
            sendChatLogin();
            return;
        }
        emit sig_con_success(true);
    });

    connect(&_socket, &QTcpSocket::disconnected, this, [this]() {
        qDebug() << "[TcpMgr] disconnected";
        if (_reconnect_in_progress) {
            finishReconnectFailure();
            return;
        }
        if (_chat_logged_in) {
            _chat_logged_in = false;
            startReconnect();
        }
    });

    connect(&_socket, &QAbstractSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError err) {
                qDebug() << "[TcpMgr] errorOccurred:" << _socket.errorString();
                if (_reconnect_in_progress) {
                    switch (err) {
                    case QAbstractSocket::ConnectionRefusedError:
                    case QAbstractSocket::HostNotFoundError:
                    case QAbstractSocket::SocketTimeoutError:
                    case QAbstractSocket::RemoteHostClosedError:
                    case QAbstractSocket::NetworkError:
                        finishReconnectFailure();
                        return;
                    default:
                        break;
                    }
                }

                switch (err) {
                case QAbstractSocket::ConnectionRefusedError:
                case QAbstractSocket::HostNotFoundError:
                case QAbstractSocket::SocketTimeoutError:
                    emit sig_con_success(false);
                    break;
                default:
                    break;
                }
            });

    connect(&_socket, &QTcpSocket::readyRead, this, [this]() {
        _buffer.append(_socket.readAll());

        forever {
            if (!_b_recv_pending) {
                constexpr int headerSize = static_cast<int>(sizeof(quint16) * 2);
                if (_buffer.size() < headerSize) {
                    return;
                }

                QDataStream stream(_buffer);
                stream.setByteOrder(QDataStream::BigEndian);
                stream >> _message_id >> _message_len;
                _buffer = _buffer.mid(headerSize);
            }

            if (_buffer.size() < static_cast<int>(_message_len)) {
                _b_recv_pending = true;
                return;
            }

            _b_recv_pending = false;
            const QByteArray body = _buffer.left(_message_len);
            _buffer = _buffer.mid(_message_len);
            dispatchMsg(_message_id, body);
        }
    });

    connect(this, &TcpMgr::sig_send_data, this, &TcpMgr::slot_send_data);
    initHandlers();
}

bool TcpMgr::isChatAvailable() const
{
    return _chat_logged_in && _socket.state() == QAbstractSocket::ConnectedState;
}

void TcpMgr::initHandlers()
{
    _handlers[ID_CHAT_LOGIN_RSP] = [this](ReqId id, int len, QByteArray data) {
        Q_UNUSED(id)
        Q_UNUSED(len)

        const QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if (jsonDoc.isNull() || !jsonDoc.isObject()) {
            _chat_logged_in = false;
            if (_reconnect_in_progress) {
                finishReconnectFailure();
            } else {
                emit sig_login_failed(ErrorCodes::ERR_JSON);
            }
            return;
        }

        const QJsonObject jsonObj = jsonDoc.object();
        if (!jsonObj.contains("error")) {
            _chat_logged_in = false;
            if (_reconnect_in_progress) {
                finishReconnectFailure();
            } else {
                emit sig_login_failed(ErrorCodes::ERR_JSON);
            }
            return;
        }

        const int err = jsonObj.value("error").toInt();
        if (err != ErrorCodes::SUCCESS) {
            _chat_logged_in = false;
            if (_reconnect_in_progress) {
                finishReconnectFailure();
            } else {
                emit sig_login_failed(err);
            }
            return;
        }

        auto &userMgr = UserMgr::getInstance();
        userMgr.SetUid(jsonObj.value("uid").toInt());
        userMgr.SetName(jsonObj.value("name").toString());
        userMgr.SetToken(jsonObj.value("token").toString());
        _chat_logged_in = true;

        if (_reconnect_in_progress) {
            qDebug() << "[TcpMgr] reconnect login success";
            resetReconnectState();
            return;
        }

        emit sig_switch_chatdlg();
    };

    auto jsonForwarder = [this](ReqId targetId, const QByteArray &data, auto signalEmitter) {
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if (jsonDoc.isNull() || !jsonDoc.isObject()) {
            qDebug() << "[TcpMgr] JSON 解析失败, msgId:" << targetId;
            return;
        }
        signalEmitter(jsonDoc.object());
    };

    _handlers[ID_SEARCH_USER_RSP] = [this, jsonForwarder](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len)
        jsonForwarder(id, data, [this](const QJsonObject &obj) { emit sig_search_user_rsp(obj); });
    };

    _handlers[ID_ADD_FRIEND_RSP] = [this, jsonForwarder](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len)
        jsonForwarder(id, data, [this](const QJsonObject &obj) { emit sig_add_friend_rsp(obj); });
    };

    _handlers[ID_GET_FRIEND_REQUESTS_RSP] = [this, jsonForwarder](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len)
        jsonForwarder(id, data, [this](const QJsonObject &obj) { emit sig_friend_requests_rsp(obj); });
    };

    _handlers[ID_HANDLE_FRIEND_REQUEST_RSP] = [this, jsonForwarder](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len)
        jsonForwarder(id, data, [this](const QJsonObject &obj) { emit sig_handle_friend_request_rsp(obj); });
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
        jsonForwarder(id, data, [this](const QJsonObject &obj) { emit sig_friend_list_push(obj); });
    };

    _handlers[ID_GET_FRIEND_LIST_RSP] = [this, jsonForwarder](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len)
        jsonForwarder(id, data, [this](const QJsonObject &obj) {
            emit sig_friend_list_rsp(obj);
            emit sig_friend_list_push(obj);
        });
    };

    _handlers[ID_GET_PRIVATE_MESSAGES_RSP] = [this, jsonForwarder](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len)
        jsonForwarder(id, data, [this](const QJsonObject &obj) { emit sig_private_messages_rsp(obj); });
    };

    _handlers[ID_SEND_PRIVATE_MESSAGE_RSP] = [this, jsonForwarder](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len)
        jsonForwarder(id, data, [this](const QJsonObject &obj) { emit sig_send_private_message_rsp(obj); });
    };

    _handlers[ID_PRIVATE_MESSAGE_PUSH] = [this, jsonForwarder](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len)
        jsonForwarder(id, data, [this](const QJsonObject &obj) { emit sig_private_message_push(obj); });
    };

    _handlers[ID_MARK_PRIVATE_MESSAGES_READ_RSP] = [this, jsonForwarder](ReqId id, int len, QByteArray data) {
        Q_UNUSED(len)
        jsonForwarder(id, data, [this](const QJsonObject &obj) { emit sig_mark_private_messages_read_rsp(obj); });
    };
}

void TcpMgr::slot_tcp_connect(ServerInfo si)
{
    qDebug() << "[TcpMgr] slot_tcp_connect:" << si.Host << si.Port;
    _host = si.Host;
    _port = static_cast<quint16>(si.Port.toUInt());
    _buffer.clear();
    _b_recv_pending = false;
    _socket.abort();
    _socket.connectToHost(_host, _port);
}

void TcpMgr::slot_send_data(ReqId reqId, QString data)
{
    const QByteArray body = data.toUtf8();
    const quint16 id = static_cast<quint16>(reqId);
    const quint16 len = static_cast<quint16>(body.size());

    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::BigEndian);
    out << id << len;
    packet.append(body);
    _socket.write(packet);
}

void TcpMgr::dispatchMsg(quint16 msgId, const QByteArray &body)
{
    const ReqId id = static_cast<ReqId>(msgId);
    auto it = _handlers.find(id);
    if (it == _handlers.end()) {
        qDebug() << "[TcpMgr] 未找到消息处理器, msgId:" << msgId;
        return;
    }
    it.value()(id, body.size(), body);
}

void TcpMgr::startReconnect()
{
    if (_reconnect_attempted || _host.isEmpty() || _port == 0) {
        emit sig_server_closed();
        return;
    }

    qDebug() << "[TcpMgr] start reconnect:" << _host << _port;
    _reconnect_attempted = true;
    _reconnect_in_progress = true;
    _buffer.clear();
    _b_recv_pending = false;
    _socket.abort();
    _socket.connectToHost(_host, _port);
}

void TcpMgr::resetReconnectState()
{
    _reconnect_in_progress = false;
    _reconnect_attempted = false;
}

void TcpMgr::finishReconnectFailure()
{
    if (!_reconnect_in_progress) {
        return;
    }

    qDebug() << "[TcpMgr] reconnect failed";
    resetReconnectState();
    emit sig_server_closed();
}

void TcpMgr::sendChatLogin()
{
    QJsonObject jsonObj;
    jsonObj["uid"] = UserMgr::getInstance().GetUid();
    jsonObj["token"] = UserMgr::getInstance().GetToken();
    emit sig_send_data(ReqId::ID_CHAT_LOGIN,
                       QString::fromUtf8(QJsonDocument(jsonObj).toJson(QJsonDocument::Indented)));
}
