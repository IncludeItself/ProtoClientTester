#ifndef PROTOCLIENT_H
#define PROTOCLIENT_H

#include <QObject>
#include <QTimer>
#include <QMap>  // 添加这行
#include <QString>  // 添加这行
#include "networkmanager.h"
#include "sessionmanager.h"
#include "protoc/data_proto.pb.h"

class ProtoClient : public QObject
{
    Q_OBJECT

public:
    explicit ProtoClient(QObject *parent = nullptr);
    ~ProtoClient();

    bool connectToServer(const QString &host, quint16 port);
    void disconnectFromServer();
    bool isConnected() const;

    // 添加自动重连设置方法
    void setAutoReconnect(bool enable, int interval = 5000);

    void login(const QString &username, const QString &passwordHash,
               const QString &deviceInfo = "", const QString &appVersion = "");
    void logout();
    void autoLogin();

    void saveSourceCode(const QString &codeId, const QString &language,
                        const QString &sourceCode, const QString &codeName = "",
                        const QString &description = "",
                        const QMap<QString, QString> &metadata = {});

    void compileSourceCode(const QString &codeId, const QString &compilerOptions = "",
                           bool optimize = false, const QString &targetIrVersion = "");

    void executeIrCode(const QString &irCodeId, data::ExecuteIRCodeRequest_ExecutionMode mode =
                                                data::ExecuteIRCodeRequest_ExecutionMode_JIT,
                       const QMap<QString, QString> &parameters = {}, uint32_t timeout = 30);

signals:
    void connectionStateChanged(bool connected);
    void loginResult(bool success, const QString &message);
    void saveSourceCodeResult(bool success, const QString &codeId, const QString &message);
    void compileResult(bool success, const QString &irCodeId, const QString &message);
    void executeResult(bool success, const QString &result, const QString &errorMessage);
    void errorOccurred(const QString &error);
    void notificationReceived(const QString &type, const QString &content);

private slots:
    void onMessageReceived(const data::MessageFrame &message);
    void onNetworkError(const QString &error);
    void onSessionExpired();

private:
    data::MessageFrame createBaseMessage(data::RequestType type) const;
    void handleLoginResponse(const data::LoginResponse &response);
    void handleErrorResponse(const data::ErrorResponse &response);
    void handleNotification(const data::Notification &notification);

    NetworkManager *m_networkManager;
    QTimer *m_sessionCheckTimer;
};

#endif // PROTOCLIENT_H
