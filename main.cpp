#include "mainwindow.h"
#include "localdb.h"

#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QSettings>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QString fileName = "config.ini";
    QString appPath = QCoreApplication::applicationDirPath();
    QString configPath = QDir::toNativeSeparators(appPath + QDir::separator() + fileName);
    QSettings settings(configPath, QSettings::IniFormat);
    QString gateHost = settings.value("GateServer/host").toString();
    QString gatePort = settings.value("GateServer/port").toString();
    gate_url_prefix = "http://" + gateHost + ":" + gatePort;

    if (!LocalDb::instance().init()) {
        QMessageBox::critical(nullptr,
                              QStringLiteral("本地数据库初始化失败"),
                              QStringLiteral("无法初始化客户端本地数据库：\n%1").arg(LocalDb::instance().lastError()));
        return -1;
    }

    MainWindow w;
    w.show();
    const int result = a.exec();
    LocalDb::instance().close();
    return result;
}
