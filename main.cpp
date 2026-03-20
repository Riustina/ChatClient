#include "mainwindow.h"
#include "localdb.h"

#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QMessageBox>
#include <QSettings>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/icons/app_logo.svg"));

    const QString fileName = QStringLiteral("config.ini");
    const QString appPath = QCoreApplication::applicationDirPath();
    const QString configPath = QDir::toNativeSeparators(appPath + QDir::separator() + fileName);
    QSettings settings(configPath, QSettings::IniFormat);
    const QString gateHost = settings.value(QStringLiteral("GateServer/host")).toString();
    const QString gatePort = settings.value(QStringLiteral("GateServer/port")).toString();
    gate_url_prefix = QStringLiteral("http://") + gateHost + QStringLiteral(":") + gatePort;

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
