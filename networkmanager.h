#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QDateTime>
#include <QHostAddress>
#include <QByteArray>
#include <QMutex>
#include <QWaitCondition>
#include "protoc/data_proto.pb.h"

class NetworkManager : public QObject
{
    Q_OBJECT

public:
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager();

    bool connectToServer(const QString &host, quint16 port);
    void disconnectFromServer();
    bool isConnected() const;

    bool sendMessage(const data::MessageFrame &message);
    data::MessageFrame sendRequest(const data::MessageFrame &request, int timeout = 5000);

    void setAutoReconnect(bool enable, int interval = 5000);

signals:
    void connected();
    void disconnected();
    void connectionError(const QString &error);
    void messageReceived(const data::MessageFrame &message);
    void heartbeatReceived();

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onErrorOccurred(QAbstractSocket::SocketError error);
    void onHeartbeatTimeout();
    // void onReconnectTimeout();

private:
    bool readMessage(QByteArray &data);
    void sendHeartbeat();
    void startHeartbeatTimer();
    void stopHeartbeatTimer();

    QTcpSocket *m_socket;
    QTimer *m_heartbeatTimer;
    QTimer *m_reconnectTimer;
    QHostAddress m_host;
    quint16 m_port;
    bool m_autoReconnect;
    QMutex m_mutex;
    QWaitCondition m_responseCondition;
    QMap<QString, data::MessageFrame> m_pendingResponses;
};

#endif // NETWORKMANAGER_H
