#include "sessionmanager.h"
#include <QUuid>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>

SessionManager::SessionManager(QObject *parent)
    : QObject(parent)
    , m_settings("YourCompany", "ProtoClientTester")
    , m_userRole(0)
    , m_expireTime(0)
    , m_loggedIn(false)
{
}

SessionManager::~SessionManager()
{
}

SessionManager& SessionManager::instance()
{
    static SessionManager instance;
    return instance;
}

bool SessionManager::isLoggedIn() const
{
    return m_loggedIn && QDateTime::currentSecsSinceEpoch() < m_expireTime;
}

QString SessionManager::sessionId() const
{
    return m_sessionId;
}

QString SessionManager::username() const
{
    return m_username;
}

QString SessionManager::userNickname() const
{
    return m_userNickname;
}

quint32 SessionManager::userRole() const
{
    return m_userRole;
}

quint64 SessionManager::expireTime() const
{
    return m_expireTime;
}

void SessionManager::login(const data::LoginResponse &response)
{
    m_sessionId = QString::fromStdString(response.session_id());
    m_expireTime = response.expire_time();
    m_userNickname = QString::fromStdString(response.user_nickname());
    m_userRole = response.user_role();
    m_loggedIn = response.success();

    if (m_loggedIn) {
        m_settings.setValue("session/id", m_sessionId);
        // 修复：显式转换为 qlonglong
        m_settings.setValue("session/expire", QVariant::fromValue<qlonglong>(static_cast<qlonglong>(m_expireTime)));
        m_settings.setValue("user/nickname", m_userNickname);
        m_settings.setValue("user/role", static_cast<uint>(m_userRole));
    }

    emit loginStateChanged(m_loggedIn);
}

void SessionManager::logout()
{
    m_sessionId.clear();
    m_username.clear();
    m_userNickname.clear();
    m_userRole = 0;
    m_expireTime = 0;
    m_loggedIn = false;

    m_settings.remove("session/id");
    m_settings.remove("session/expire");
    m_settings.remove("user/nickname");
    m_settings.remove("user/role");

    emit loginStateChanged(false);
}

void SessionManager::updateSession(const QString &newSessionId, quint64 newExpireTime)
{
    m_sessionId = newSessionId;
    m_expireTime = newExpireTime;

    m_settings.setValue("session/id", m_sessionId);
    // 修复：显式转换为 qlonglong
    m_settings.setValue("session/expire", QVariant::fromValue<qlonglong>(static_cast<qlonglong>(m_expireTime)));
}

void SessionManager::saveCredentials(const QString &username, const QString &passwordHash)
{
    QByteArray encrypted = QCryptographicHash::hash(
                               passwordHash.toUtf8(), QCryptographicHash::Sha256).toBase64();

    m_settings.setValue("auth/username", username);
    m_settings.setValue("auth/password", QString::fromUtf8(encrypted));
    m_settings.sync();
}

bool SessionManager::loadCredentials(QString &username, QString &passwordHash)
{
    username = m_settings.value("auth/username").toString();
    QString encrypted = m_settings.value("auth/password").toString();

    if (username.isEmpty() || encrypted.isEmpty()) {
        return false;
    }

    // 注意：这里存储的是哈希值，实际使用时需要根据服务器要求处理
    passwordHash = encrypted;
    return true;
}

void SessionManager::clearCredentials()
{
    m_settings.remove("auth/username");
    m_settings.remove("auth/password");
}

QString SessionManager::generateRequestId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
