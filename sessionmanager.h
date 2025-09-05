#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <QObject>
#include <QString>
#include <QSettings>
#include <QVariant>
#include "protoc/data_proto.pb.h"

class SessionManager : public QObject
{
    Q_OBJECT

public:
    static SessionManager& instance();

    bool isLoggedIn() const;
    QString sessionId() const;
    QString username() const;
    QString userNickname() const;
    quint32 userRole() const;  // 改为 quint32
    quint64 expireTime() const; // 改为 quint64

    void login(const data::LoginResponse &response);
    void logout();
    void updateSession(const QString &newSessionId, quint64 newExpireTime); // 改为 quint64

    void saveCredentials(const QString &username, const QString &passwordHash);
    bool loadCredentials(QString &username, QString &passwordHash);
    void clearCredentials();

    QString generateRequestId() const;

signals:
    void sessionExpired();
    void loginStateChanged(bool loggedIn);

private:
    explicit SessionManager(QObject *parent = nullptr);
    ~SessionManager();

    QSettings m_settings;
    QString m_sessionId;
    QString m_username;
    QString m_userNickname;
    quint32 m_userRole;    // 改为 quint32
    quint64 m_expireTime;  // 改为 quint64
    bool m_loggedIn;
};

#endif // SESSIONMANAGER_H
