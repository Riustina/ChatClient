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
    a.setStyleSheet(a.styleSheet() +
                    QStringLiteral(
                        "QMessageBox { background:#F4F3F9; }"
                        "QMessageBox QLabel { color:#111827; font:10pt 'Microsoft YaHei UI'; }"
                        "QMessageBox QPushButton { min-width:84px; min-height:32px; padding:0 14px; "
                        "background:#E5E7EB; color:#111827; border:none; border-radius:16px; font:10pt 'Microsoft YaHei UI'; }"
                        "QMessageBox QPushButton:hover { background:#D7DAE0; }"
                        "QMessageBox QPushButton:pressed { background:#C9CDD4; }"));

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
