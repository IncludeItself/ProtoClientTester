#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QSettings>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_client(new ProtoClient(this))
    , m_statusTimer(new QTimer(this))
{
    ui->setupUi(this);

    setupConnections();
    loadSettings();
    updateUIState();

    m_statusTimer->setInterval(1000);
    connect(m_statusTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    m_statusTimer->start();
}

MainWindow::~MainWindow()
{
    saveSettings();
    delete ui;
}

void MainWindow::setupConnections()
{
    // 连接客户端信号
    connect(m_client, &ProtoClient::connectionStateChanged,
            this, &MainWindow::onConnectionStateChanged);
    connect(m_client, &ProtoClient::loginResult,
            this, &MainWindow::onLoginResult);
    connect(m_client, &ProtoClient::saveSourceCodeResult,
            this, &MainWindow::onSaveSourceCodeResult);
    connect(m_client, &ProtoClient::compileResult,
            this, &MainWindow::onCompileResult);
    connect(m_client, &ProtoClient::executeResult,
            this, &MainWindow::onExecuteResult);
    connect(m_client, &ProtoClient::errorOccurred,
            this, &MainWindow::onErrorOccurred);
    connect(m_client, &ProtoClient::notificationReceived,
            this, &MainWindow::onNotificationReceived);
}

void MainWindow::onConnectionStateChanged(bool connected)
{
    ui->pushButtonConnect->setEnabled(!connected);
    ui->pushButtonDisconnect->setEnabled(connected);
    ui->groupBoxAuth->setEnabled(connected);

    if (connected) {
        showStatusMessage("已连接到服务器", 3000);
        // 自动登录
        if (ui->checkBoxRemember->isChecked() && !ui->lineEditUsername->text().isEmpty()) {
            QTimer::singleShot(1000, this, [this]() {
                m_client->autoLogin();
            });
        }
    } else {
        showStatusMessage("已断开连接", 3000);
        ui->tabWidget->setEnabled(false);
    }
}

void MainWindow::onLoginResult(bool success, const QString &message)
{
    if (success) {
        showStatusMessage("登录成功: " + message, 3000);
        ui->tabWidget->setEnabled(true);
    } else {
        showStatusMessage("登录失败: " + message, 5000);
        QMessageBox::warning(this, "登录失败", message);
    }
}

void MainWindow::onSaveSourceCodeResult(bool success, const QString &codeId, const QString &message)
{
    if (success) {
        m_lastCodeId = codeId;
        ui->lineEditCodeId->setText(codeId);
        showStatusMessage("保存成功: " + codeId, 3000);
    } else {
        showStatusMessage("保存失败: " + message, 5000);
        QMessageBox::warning(this, "保存失败", message);
    }
}

void MainWindow::onCompileResult(bool success, const QString &irCodeId, const QString &message)
{
    if (success) {
        ui->lineEditIrCodeId->setText(irCodeId);
        showStatusMessage("编译成功: " + irCodeId, 3000);
        ui->textEditResult->append("编译成功: " + irCodeId + "\n" + message);
    } else {
        showStatusMessage("编译失败: " + message, 5000);
        ui->textEditResult->append("编译失败: " + message);
    }
}

void MainWindow::onExecuteResult(bool success, const QString &result, const QString &errorMessage)
{
    if (success) {
        showStatusMessage("执行成功", 3000);
        ui->textEditResult->append("执行结果:\n" + result);
        ui->labelExecStatus->setText("状态: 执行成功");
    } else {
        showStatusMessage("执行失败: " + errorMessage, 5000);
        ui->textEditResult->append("执行错误: " + errorMessage);
        ui->labelExecStatus->setText("状态: 执行失败");
    }
}

void MainWindow::onErrorOccurred(const QString &error)
{
    showStatusMessage("错误: " + error, 5000);
    ui->textEditResult->append("错误: " + error);
}

void MainWindow::onNotificationReceived(const QString &type, const QString &content)
{
    QString message = QString("[%1] %2: %3")
    .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
        .arg(type)
        .arg(content);
    ui->textEditResult->append(message);
    showStatusMessage("收到通知: " + type, 3000);
}

void MainWindow::on_pushButtonConnect_clicked()
{
    QString host = ui->lineEditHost->text();
    quint16 port = static_cast<quint16>(ui->spinBoxPort->value());

    // 先设置自动重连
    m_client->setAutoReconnect(ui->checkBoxAutoReconnect->isChecked());

    if (m_client->connectToServer(host, port)) {
        showStatusMessage("正在连接服务器...", 2000);
    } else {
        QMessageBox::critical(this, "连接失败", "无法连接到服务器");
    }
}

void MainWindow::on_pushButtonDisconnect_clicked()
{
    m_client->disconnectFromServer();
}

void MainWindow::on_pushButtonLogin_clicked()
{
    QString username = ui->lineEditUsername->text();
    QString password = ui->lineEditPassword->text();
    QString deviceInfo = ui->lineEditDeviceInfo->text();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "输入错误", "用户名和密码不能为空");
        return;
    }

    m_client->login(username, password, deviceInfo, "v1.0.0");
}

void MainWindow::on_pushButtonSaveSource_clicked()
{
    QString codeId = ui->lineEditCodeId->text();
    QString language = ui->comboBoxLanguage->currentText();
    QString sourceCode = ui->textEditSourceCode->toPlainText();
    QString codeName = ui->lineEditCodeName->text();

    if (sourceCode.isEmpty()) {
        QMessageBox::warning(this, "输入错误", "源代码不能为空");
        return;
    }

    m_client->saveSourceCode(codeId, language, sourceCode, codeName);
}

void MainWindow::on_pushButtonCompile_clicked()
{
    QString codeId = ui->lineEditCodeId->text();
    if (codeId.isEmpty()) {
        QMessageBox::warning(this, "输入错误", "请先保存源代码或输入代码ID");
        return;
    }

    m_client->compileSourceCode(codeId);
}

void MainWindow::on_pushButtonExecute_clicked()
{
    QString irCodeId = ui->lineEditIrCodeId->text();
    if (irCodeId.isEmpty()) {
        QMessageBox::warning(this, "输入错误", "请输入IR代码ID");
        return;
    }

    data::ExecuteIRCodeRequest_ExecutionMode mode;
    switch (ui->comboBoxExecMode->currentIndex()) {
    case 0: mode = data::ExecuteIRCodeRequest_ExecutionMode_JIT; break;
    case 1: mode = data::ExecuteIRCodeRequest_ExecutionMode_INTERPRET; break;
    case 2: mode = data::ExecuteIRCodeRequest_ExecutionMode_BOTH; break;
    default: mode = data::ExecuteIRCodeRequest_ExecutionMode_JIT;
    }

    uint32_t timeout = static_cast<uint32_t>(ui->spinBoxTimeout->value());

    m_client->executeIrCode(irCodeId, mode, {}, timeout);
}

void MainWindow::on_pushButtonClearResult_clicked()
{
    ui->textEditResult->clear();
    ui->labelExecTime->setText("执行时间: 0ms");
    ui->labelExecStatus->setText("状态: 未执行");
}

void MainWindow::on_pushButtonLoadSource_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, "打开源代码文件", "", "All Files (*)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ui->textEditSourceCode->setPlainText(file.readAll());
        file.close();
    }
}

void MainWindow::updateStatusBar()
{
    QString status;
    if (m_client->isConnected()) {
        status = "已连接";
        if (SessionManager::instance().isLoggedIn()) {
            status += " | 已登录: " + SessionManager::instance().userNickname();

            // 显示会话剩余时间
            quint64 remaining = (SessionManager::instance().expireTime() - QDateTime::currentMSecsSinceEpoch()) / 1000;
            if (remaining > 0) {
                status += QString(" | 剩余: %1秒").arg(remaining);
            }
        } else {
            status += " | 未登录";
        }
    } else {
        status = "未连接";
    }

    status += " | " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    ui->statusBar->showMessage(status);
}

void MainWindow::updateUIState()
{
    bool connected = m_client->isConnected();
    bool loggedIn = SessionManager::instance().isLoggedIn();

    ui->pushButtonConnect->setEnabled(!connected);
    ui->pushButtonDisconnect->setEnabled(connected);
    ui->groupBoxAuth->setEnabled(connected);
    ui->tabWidget->setEnabled(connected && loggedIn);
}

void MainWindow::loadSettings()
{
    QSettings settings("YourCompany", "ProtoClientTester");

    ui->lineEditHost->setText(settings.value("connection/host", "127.0.0.1").toString());
    ui->spinBoxPort->setValue(settings.value("connection/port", 8080).toInt());
    ui->checkBoxAutoReconnect->setChecked(settings.value("connection/autoReconnect", true).toBool());

    ui->lineEditUsername->setText(settings.value("auth/username").toString());
    ui->checkBoxRemember->setChecked(settings.value("auth/remember", false).toBool());

    ui->comboBoxLanguage->setCurrentText(settings.value("editor/language", "python").toString());
}

void MainWindow::saveSettings()
{
    QSettings settings("YourCompany", "ProtoClientTester");

    settings.setValue("connection/host", ui->lineEditHost->text());
    settings.setValue("connection/port", ui->spinBoxPort->value());
    settings.setValue("connection/autoReconnect", ui->checkBoxAutoReconnect->isChecked());

    if (ui->checkBoxRemember->isChecked()) {
        settings.setValue("auth/username", ui->lineEditUsername->text());
    }
    settings.setValue("auth/remember", ui->checkBoxRemember->isChecked());

    settings.setValue("editor/language", ui->comboBoxLanguage->currentText());
}

void MainWindow::showStatusMessage(const QString &message, int timeout)
{
    ui->statusBar->showMessage(message, timeout);
}
