#include "networkmanager.h"
#include <google/protobuf/util/json_util.h>
#include <QThread>
#include <QDebug>
#include <QUuid>

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_heartbeatTimer(new QTimer(this))
    , m_reconnectTimer(new QTimer(this))
    , m_port(0)
    , m_autoReconnect(false)
{
    connect(m_socket, &QTcpSocket::connected, this, &NetworkManager::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetworkManager::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &NetworkManager::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &NetworkManager::onErrorOccurred);

    m_heartbeatTimer->setInterval(30000);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &NetworkManager::onHeartbeatTimeout);

    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, [this]() {
        qInfo() << "Attempting to reconnect to server...";
        connectToServer(m_host.toString(), m_port);
    });
}

NetworkManager::~NetworkManager()
{
    disconnectFromServer();
}

bool NetworkManager::connectToServer(const QString &host, quint16 port)
{
    m_host = QHostAddress(host);
    m_port = port;

    m_socket->connectToHost(host, port);
    return m_socket->waitForConnected(5000);
}

void NetworkManager::disconnectFromServer()
{
    stopHeartbeatTimer();
    m_socket->disconnectFromHost();
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->waitForDisconnected(1000);
    }
}

bool NetworkManager::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

bool NetworkManager::sendMessage(const data::MessageFrame &message)
{
    if (!isConnected()) {
        qWarning() << "Not connected to server";
        return false;
    }

    QByteArray data;
    data.append("PBFRAME");
    quint32 size = message.ByteSizeLong();
    data.append(reinterpret_cast<const char*>(&size), sizeof(size));

    std::string serialized;
    if (!message.SerializeToString(&serialized)) {
        qWarning() << "Failed to serialize message";
        return false;
    }

    data.append(serialized.c_str(), serialized.size());

    qint64 bytesWritten = m_socket->write(data);
    if (bytesWritten == -1) {
        qWarning() << "Failed to write data to socket:" << m_socket->errorString();
        return false;
    }

    return m_socket->waitForBytesWritten(5000);
}

data::MessageFrame NetworkManager::sendRequest(const data::MessageFrame &request, int timeout)
{
    QMutexLocker locker(&m_mutex);

    if (!sendMessage(request)) {
        data::MessageFrame errorResponse;
        auto* header = errorResponse.mutable_header();
        header->set_request_id(request.header().request_id());
        header->set_timestamp(QDateTime::currentSecsSinceEpoch());
        header->set_type(data::ERROR_RESPONSE);

        return errorResponse;
    }

    QString requestId = QString::fromStdString(request.header().request_id());

    if (!m_responseCondition.wait(&m_mutex, timeout)) {
        qWarning() << "Request timeout for request ID:" << requestId;
        data::MessageFrame timeoutResponse;
        auto* header = timeoutResponse.mutable_header();
        header->set_request_id(request.header().request_id());
        header->set_timestamp(QDateTime::currentSecsSinceEpoch());
        header->set_type(data::ERROR_RESPONSE);

        return timeoutResponse;
    }

    return m_pendingResponses.value(requestId);
}

void NetworkManager::setAutoReconnect(bool enable, int interval)
{
    m_autoReconnect = enable;
    if (enable) {
        m_reconnectTimer->setInterval(interval);
    } else {
        m_reconnectTimer->stop();
    }
}

void NetworkManager::onConnected()
{
    qInfo() << "Connected to server";
    startHeartbeatTimer();
    emit connected();
}

void NetworkManager::onDisconnected()
{
    qInfo() << "Disconnected from server";
    stopHeartbeatTimer();
    emit disconnected();

    if (m_autoReconnect) {
        m_reconnectTimer->start();
    }
}

void NetworkManager::onReadyRead()
{
    QByteArray data = m_socket->readAll();
    static QByteArray buffer;
    buffer.append(data);

    while (buffer.size() >= 11) { // 7 bytes magic + 4 bytes size
        if (buffer.left(7) != "PBFRAME") {
            qWarning() << "Invalid frame magic";
            buffer.clear();
            return;
        }

        quint32 frameSize;
        memcpy(&frameSize, buffer.constData() + 7, sizeof(frameSize));

        if (buffer.size() < 11 + frameSize) {
            // 数据不完整，等待更多数据
            return;
        }

        QByteArray messageData = buffer.mid(11, frameSize);
        buffer.remove(0, 11 + frameSize);

        data::MessageFrame message;
        if (message.ParseFromArray(messageData.constData(), messageData.size())) {
            QString requestId = QString::fromStdString(message.header().request_id());

            QMutexLocker locker(&m_mutex);
            if (m_pendingResponses.contains(requestId)) {
                m_pendingResponses[requestId] = message;
                m_responseCondition.wakeAll();
            } else {
                emit messageReceived(message);
            }

            if (message.header().type() == data::HEARTBEAT) {
                emit heartbeatReceived();
            }
        } else {
            qWarning() << "Failed to parse message";
        }
    }
}

void NetworkManager::onErrorOccurred(QAbstractSocket::SocketError error)
{
    qWarning() << "Socket error:" << m_socket->errorString();
    emit connectionError(m_socket->errorString());
}

void NetworkManager::onHeartbeatTimeout()
{
    sendHeartbeat();
}

void NetworkManager::sendHeartbeat()
{
    data::MessageFrame message;
    auto* header = message.mutable_header();
    header->set_request_id(QUuid::createUuid().toString().toStdString());
    header->set_timestamp(QDateTime::currentMSecsSinceEpoch()); // 改为毫秒
    header->set_type(data::HEARTBEAT);

    data::Heartbeat heartbeat;
    heartbeat.set_last_active_time(QDateTime::currentMSecsSinceEpoch()); // 改为毫秒
    message.mutable_heartbeat()->CopyFrom(heartbeat);

    sendMessage(message);
}

void NetworkManager::startHeartbeatTimer()
{
    m_heartbeatTimer->start();
    // 立即发送第一个心跳
    QTimer::singleShot(0, this, &NetworkManager::sendHeartbeat);
}

void NetworkManager::stopHeartbeatTimer()
{
    m_heartbeatTimer->stop();
}
