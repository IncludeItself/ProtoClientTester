#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include "protoclient.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onConnectionStateChanged(bool connected);
    void onLoginResult(bool success, const QString &message);
    void onSaveSourceCodeResult(bool success, const QString &codeId, const QString &message);
    void onCompileResult(bool success, const QString &irCodeId, const QString &message);
    void onExecuteResult(bool success, const QString &result, const QString &errorMessage);
    void onErrorOccurred(const QString &error);
    void onNotificationReceived(const QString &type, const QString &content);

    void on_pushButtonConnect_clicked();
    void on_pushButtonDisconnect_clicked();
    void on_pushButtonLogin_clicked();
    void on_pushButtonSaveSource_clicked();
    void on_pushButtonCompile_clicked();
    void on_pushButtonExecute_clicked();
    void on_pushButtonClearResult_clicked();
    void on_pushButtonLoadSource_clicked();

    void updateStatusBar();

private:
    void setupConnections();
    void updateUIState();
    void loadSettings();
    void saveSettings();
    void showStatusMessage(const QString &message, int timeout = 5000);

    Ui::MainWindow *ui;
    ProtoClient *m_client;
    QTimer *m_statusTimer;
    QString m_lastCodeId;
};

#endif // MAINWINDOW_H
