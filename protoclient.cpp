#include "protoclient.h"
#include <QDateTime>
#include <QDebug>

ProtoClient::ProtoClient(QObject *parent)
    : QObject(parent)
    , m_networkManager(new NetworkManager(this))
    , m_sessionCheckTimer(new QTimer(this))
{
    // 连接消息接收信号
    connect(m_networkManager, &NetworkManager::messageReceived,
            this, &ProtoClient::onMessageReceived);

    // 连接错误信号
    connect(m_networkManager, &NetworkManager::connectionError,
            this, &ProtoClient::onNetworkError);

    // 修复：使用 lambda 表达式来处理连接状态变化
    connect(m_networkManager, &NetworkManager::connected, this, [this]() {
        emit connectionStateChanged(true);
    });

    connect(m_networkManager, &NetworkManager::disconnected, this, [this]() {
        emit connectionStateChanged(false);
    });

    m_sessionCheckTimer->setInterval(60000); // 每分钟检查一次会话
    connect(m_sessionCheckTimer, &QTimer::timeout, this, [this]() {
        if (SessionManager::instance().isLoggedIn() &&
            QDateTime::currentMSecsSinceEpoch() > SessionManager::instance().expireTime() - 300000) {
            onSessionExpired();
        }
    });
}

ProtoClient::~ProtoClient()
{
    disconnectFromServer();
}

bool ProtoClient::connectToServer(const QString &host, quint16 port)
{
    return m_networkManager->connectToServer(host, port);
}

void ProtoClient::disconnectFromServer()
{
    m_networkManager->disconnectFromServer();
    m_sessionCheckTimer->stop();
}

bool ProtoClient::isConnected() const
{
    return m_networkManager->isConnected();
}

// 添加 setAutoReconnect 方法的实现
void ProtoClient::setAutoReconnect(bool enable, int interval)
{
    m_networkManager->setAutoReconnect(enable, interval);
}

void ProtoClient::login(const QString &username, const QString &passwordHash,
                        const QString &deviceInfo, const QString &appVersion)
{
    data::MessageFrame message = createBaseMessage(data::LOGIN_REQUEST);

    data::LoginRequest loginRequest;
    loginRequest.set_username(username.toStdString());
    loginRequest.set_password_hash(passwordHash.toStdString());
    loginRequest.set_device_info(deviceInfo.toStdString());
    loginRequest.set_app_version(appVersion.toStdString());

    message.mutable_login_request()->CopyFrom(loginRequest);

    if (!m_networkManager->sendMessage(message)) {
        emit loginResult(false, "发送登录请求失败");
    }
}

void ProtoClient::logout()
{
    SessionManager::instance().logout();
    m_sessionCheckTimer->stop();
}

void ProtoClient::autoLogin()
{
    QString username, passwordHash;
    if (SessionManager::instance().loadCredentials(username, passwordHash)) {
        login(username, passwordHash);
    }
}

void ProtoClient::saveSourceCode(const QString &codeId, const QString &language,
                                 const QString &sourceCode, const QString &codeName,
                                 const QString &description, const QMap<QString, QString> &metadata)
{
    data::MessageFrame message = createBaseMessage(data::SAVE_SOURCE_CODE_REQUEST);

    data::SaveSourceCodeRequest request;
    request.set_code_id(codeId.toStdString());
    request.set_language(language.toStdString());
    request.set_source_code(sourceCode.toStdString());
    request.set_code_name(codeName.toStdString());
    request.set_description(description.toStdString());

    for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it) {
        (*request.mutable_metadata())[it.key().toStdString()] = it.value().toStdString();
    }

    message.mutable_save_source_request()->CopyFrom(request);

    if (!m_networkManager->sendMessage(message)) {
        emit saveSourceCodeResult(false, "", "发送保存请求失败");
    }
}

void ProtoClient::compileSourceCode(const QString &codeId, const QString &compilerOptions,
                                    bool optimize, const QString &targetIrVersion)
{
    data::MessageFrame message = createBaseMessage(data::COMPILE_SOURCE_REQUEST);

    data::CompileSourceCodeRequest request;
    request.set_code_id(codeId.toStdString());
    request.set_compiler_options(compilerOptions.toStdString());
    request.set_optimize(optimize);
    request.set_target_ir_version(targetIrVersion.toStdString());

    message.mutable_compile_request()->CopyFrom(request);

    if (!m_networkManager->sendMessage(message)) {
        emit compileResult(false, "", "发送编译请求失败");
    }
}

void ProtoClient::executeIrCode(const QString &irCodeId, data::ExecuteIRCodeRequest_ExecutionMode mode,
                                const QMap<QString, QString> &parameters, uint32_t timeout)
{
    data::MessageFrame message = createBaseMessage(data::EXECUTE_IR_REQUEST);

    data::ExecuteIRCodeRequest request;
    request.set_ir_code_id(irCodeId.toStdString());
    request.set_mode(mode);
    request.set_timeout(timeout);

    for (auto it = parameters.constBegin(); it != parameters.constEnd(); ++it) {
        (*request.mutable_parameters())[it.key().toStdString()] = it.value().toStdString();
    }

    message.mutable_execute_ir_request()->CopyFrom(request);

    if (!m_networkManager->sendMessage(message)) {
        emit executeResult(false, "", "发送执行请求失败");
    }
}

void ProtoClient::onMessageReceived(const data::MessageFrame &message)
{
    switch (message.header().type()) {
    case data::LOGIN_RESPONSE:
        handleLoginResponse(message.login_response());
        break;
    case data::SAVE_SOURCE_CODE_RESPONSE:
        emit saveSourceCodeResult(message.save_source_response().success(),
                                  QString::fromStdString(message.save_source_response().code_id()),
                                  QString::fromStdString(message.save_source_response().message()));
        break;
    case data::COMPILE_SOURCE_RESPONSE:
        emit compileResult(message.compile_response().success(),
                           QString::fromStdString(message.compile_response().ir_code_id()),
                           QString::fromStdString(message.compile_response().message()));
        break;
    case data::EXECUTE_IR_RESPONSE:
        emit executeResult(message.execute_ir_response().success(),
                           QString::fromStdString(message.execute_ir_response().execution_result()),
                           QString::fromStdString(message.execute_ir_response().error_message()));
        break;
    case data::ERROR_RESPONSE:
        handleErrorResponse(message.error_response());
        break;
    case data::NOTIFICATION:
        handleNotification(message.notification());
        break;
    default:
        qWarning() << "Received unknown message type:" << message.header().type();
        break;
    }
}

void ProtoClient::onNetworkError(const QString &error)
{
    emit errorOccurred(error);
}

void ProtoClient::onSessionExpired()
{
    emit errorOccurred("会话已过期，请重新登录");
    logout();
}

data::MessageFrame ProtoClient::createBaseMessage(data::RequestType type) const
{
    data::MessageFrame message;
    auto* header = message.mutable_header();

    header->set_request_id(SessionManager::instance().generateRequestId().toStdString());
    header->set_client_id("ProtoClientTester");
    header->set_timestamp(QDateTime::currentMSecsSinceEpoch());
    header->set_type(type);

    if (SessionManager::instance().isLoggedIn()) {
        header->set_auth_token(SessionManager::instance().sessionId().toStdString());
    }

    return message;
}

void ProtoClient::handleLoginResponse(const data::LoginResponse &response)
{
    bool success = response.success();
    QString message = QString::fromStdString(response.session_id());

    if (success) {
        SessionManager::instance().login(response);
        m_sessionCheckTimer->start();
    }

    emit loginResult(success, message);
}

void ProtoClient::handleErrorResponse(const data::ErrorResponse &response)
{
    QString errorCodeStr;
    if (response.has_common_code()) {
        errorCodeStr = QString("common_code: %1").arg(static_cast<int>(response.common_code()));
    } else if (response.has_network_code()) {
        errorCodeStr = QString("network_code: %1").arg(static_cast<int>(response.network_code()));
    } else {
        errorCodeStr = "未知错误码";
    }

    QString errorMsg = QString("错误代码: %1 - %2")
                           .arg(errorCodeStr)
                           .arg(QString::fromStdString(response.message()));

    // 如果有详细信息，添加到错误消息中
    if (!response.detail().empty()) {
        errorMsg += "\n详细信息: " + QString::fromStdString(response.detail());
    }

    // 如果有建议解决方案，添加到错误消息中
    if (!response.solution().empty()) {
        errorMsg += "\n解决方案: " + QString::fromStdString(response.solution());
    }

    // 检查是否是认证失败相关错误（这里假设common.ErrorCode中包含AUTH_FAILED）
    if (response.has_common_code() && response.common_code() == common::AUTH_FAILED) {
        onSessionExpired();
    }

    emit errorOccurred(errorMsg);
}

void ProtoClient::handleNotification(const data::Notification &notification)
{
    QString type;
    switch (notification.type()) {
    case data::Notification::SYSTEM_ANNOUNCEMENT:
        type = "系统公告";
        break;
    case data::Notification::ORDER_STATUS_CHANGE:
        type = "订单状态变更";
        break;
    case data::Notification::FRIEND_REQUEST:
        type = "好友请求";
        break;
    default:
        type = "未知通知";
        break;
    }

    emit notificationReceived(type, QString::fromStdString(notification.content()));
}
