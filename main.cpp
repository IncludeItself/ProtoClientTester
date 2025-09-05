#include "mainwindow.h"
#include <QApplication>
#include <QStyleFactory>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>

void setupApplication()
{
    // 设置应用程序信息
    QApplication::setApplicationName("ProtoClientTester");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("YourCompany");
    QApplication::setOrganizationDomain("yourcompany.com");

    // 设置样式
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    // 创建日志目录
    QDir logDir("logs");
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 设置应用程序
    setupApplication();

    // 初始化protobuf
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    // 创建主窗口
    MainWindow w;
    w.show();

    int ret = a.exec();

    // 清理protobuf
    google::protobuf::ShutdownProtobufLibrary();

    return ret;
}
