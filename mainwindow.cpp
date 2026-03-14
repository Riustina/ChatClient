#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "tcpmgr.h"
#include "usermgr.h"
#include <QEvent>
#include <QMessageBox>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->statusbar->setSizeGripEnabled(false);
    ui->statusbar->hide();

    _stackedWidget = new QStackedWidget(this);
    setCentralWidget(_stackedWidget);

    _chatPage = new ChatPage(this);
    _loginDialog = new LoginDialog(this);
    _registerDialog = new RegisterDialog(this);
    _resetDialog = new ResetDialog(this);
    _stackedWidget->addWidget(_chatPage);
    _stackedWidget->addWidget(_loginDialog);
    _stackedWidget->addWidget(_registerDialog);
    _stackedWidget->addWidget(_resetDialog);
    _stackedWidget->setCurrentWidget(_loginDialog);

    resize(1110, 730);
    setMinimumSize(880, 590);
    setWindowTitle("Chat Client");

    // 绑定信号和槽
    connect(_loginDialog, &LoginDialog::switchRegister, this, [this]() {
        _stackedWidget->setCurrentWidget(_registerDialog);
    });
    connect(_registerDialog, &RegisterDialog::switchToLogin, this, [this]() {
        _stackedWidget->setCurrentWidget(_loginDialog);
    });

    connect(_loginDialog, &LoginDialog::switchReset, this, [this]() {
        _stackedWidget->setCurrentWidget(_resetDialog);
    });
    connect(_resetDialog, &ResetDialog::switchToLogin, this, [this]() {
        _stackedWidget->setCurrentWidget(_loginDialog);
    });

    connect(&TcpMgr::getInstance(), &TcpMgr::sig_switch_chatdlg, this, [this]() {
        _chatPage->setCurrentUser(UserMgr::getInstance().GetUid(),
                                  UserMgr::getInstance().GetName());
        setWindowTitle(UserMgr::getInstance().GetName());
        _stackedWidget->setCurrentWidget(_chatPage);
    });

    connect(&TcpMgr::getInstance(), &TcpMgr::sig_server_closed, this, [this]() {
        QMessageBox::warning(this, "连接断开", "聊天服务器已关闭或连接已断开，请重新登录。", QMessageBox::Ok);
        setFriendRequestTaskbarFlash(false);
        setWindowTitle("Chat Client");
        _stackedWidget->setCurrentWidget(_loginDialog);
    });

    connect(_chatPage, &ChatPage::friendRequestNotificationChanged, this, [this](bool hasUnread) {
        setFriendRequestTaskbarFlash(hasUnread);
    });
}

MainWindow::~MainWindow()
{
    delete ui;

}

bool MainWindow::event(QEvent *event)
{
    if (event->type() == QEvent::WindowActivate && _friendRequestFlashActive) {
        setFriendRequestTaskbarFlash(false);
    }
    return QMainWindow::event(event);
}

void MainWindow::setFriendRequestTaskbarFlash(bool enabled)
{
    _friendRequestFlashActive = enabled;
#ifdef Q_OS_WIN
    FLASHWINFO info;
    info.cbSize = sizeof(FLASHWINFO);
    info.hwnd = reinterpret_cast<HWND>(winId());
    info.dwFlags = enabled ? (FLASHW_TRAY | FLASHW_TIMERNOFG) : FLASHW_STOP;
    info.uCount = enabled ? 0 : 0;
    info.dwTimeout = 0;
    FlashWindowEx(&info);
#else
    Q_UNUSED(enabled);
#endif
}
