// TcpMgr.cpp

#include "TcpMgr.h"
#include <QDataStream>
#include <QDebug>

TcpMgr::TcpMgr()
    : _host("")
    , _port(0)
    , _b_recv_pending(false)
    , _message_id(0)
    , _message_len(0)
{
    // ── 连接成功 ──────────────────────────────────
    connect(&_socket, &QTcpSocket::connected, this, [this]() {
        qDebug() << "[TcpMgr] connected: 连接服务器成功";
        emit sig_con_success(true);
    });

    // ── 断开连接 ──────────────────────────────────
    connect(&_socket, &QTcpSocket::disconnected, this, [this]() {
        qDebug() << "[TcpMgr] disconnected: 与服务器断开连接";
    });

    // ── 错误处理（Qt6 用 errorOccurred）────────────
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

    // ── 收到数据：处理粘包 ─────────────────────────
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

    // ── 自身 sig_send_data → slot_send_data ───────
    // 保证跨线程调用发送时也能回到 TcpMgr 所在线程执行
    connect(this, &TcpMgr::sig_send_data, this, &TcpMgr::slot_send_data);
}

// ─────────────────────────────────────────────
// 连接服务器
// ─────────────────────────────────────────────
void TcpMgr::slot_tcp_connect(ServerInfo si)
{
    qDebug() << "[TcpMgr] slot_tcp_connect: 正在连接"
             << si.Host << ":" << si.Port;

    _host = si.Host;
    _port = static_cast<quint16>(si.Port.toUInt());
    _socket.connectToHost(_host, _port);
}

// ─────────────────────────────────────────────
// 打包并发送数据
// 包格式：[msgId: 2字节][msgLen: 2字节][body: N字节]
// msgLen 是 body 的字节长度（UTF-8）
// ─────────────────────────────────────────────
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

// ─────────────────────────────────────────────
// 根据 msgId 派发消息（后续在这里扩展 handler）
// ─────────────────────────────────────────────
void TcpMgr::dispatchMsg(quint16 msgId, const QByteArray& body)
{
    // TODO: 后续改成 QMap<quint16, handler> 的形式统一派发
    qDebug() << "[TcpMgr] dispatchMsg: msgId=" << msgId
             << "  暂未注册 handler";
    Q_UNUSED(body)
}
