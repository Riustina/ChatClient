#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "localdb.h"
#include "tcpmgr.h"
#include "usermgr.h"
#include <QEvent>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSizePolicy>

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

    _contentStack = new QStackedWidget(this);
    setCentralWidget(_contentStack);

    _chatPage = new ChatPage(this);
    _loginDialog = new LoginDialog();
    _registerDialog = new RegisterDialog();
    _resetDialog = new ResetDialog();

    auto createAuthPage = [](QWidget *dialog) {
        auto *page = new QWidget();
        page->setAttribute(Qt::WA_StyledBackground, true);
        page->setStyleSheet("background:transparent;");
        auto *pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(0, 0, 0, 0);
        pageLayout->addStretch();
        auto *row = new QHBoxLayout();
        row->setContentsMargins(0, 0, 0, 0);
        row->addStretch();
        row->addWidget(dialog);
        row->addStretch();
        pageLayout->addLayout(row);
        pageLayout->addStretch();
        return page;
    };

    _authContainer = new QWidget(this);
    _authContainer->setStyleSheet("background:#F7F5FB;");
    auto *authOuter = new QVBoxLayout(_authContainer);
    authOuter->setContentsMargins(20, 10, 20, 10);
    authOuter->addStretch();
    auto *authRow = new QHBoxLayout();
    authRow->addStretch();
    _authStack = new QStackedWidget(_authContainer);
    _authStack->setFixedSize(520, 590);
    _authStack->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    _authStack->setStyleSheet("background:transparent;");
    authRow->addWidget(_authStack);
    authRow->addStretch();
    authOuter->addLayout(authRow);
    authOuter->addStretch();

    _loginDialog->setParent(_authStack);
    _registerDialog->setParent(_authStack);
    _resetDialog->setParent(_authStack);
    _loginDialog->setFixedSize(460, 520);
    _registerDialog->setFixedSize(460, 520);
    _resetDialog->setFixedSize(460, 520);

    auto *loginPage = createAuthPage(_loginDialog);
    auto *registerPage = createAuthPage(_registerDialog);
    auto *resetPage = createAuthPage(_resetDialog);
    _authStack->addWidget(loginPage);
    _authStack->addWidget(registerPage);
    _authStack->addWidget(resetPage);
    _authStack->setCurrentWidget(loginPage);

    _contentStack->addWidget(_authContainer);
    _contentStack->addWidget(_chatPage);
    _contentStack->setCurrentWidget(_authContainer);

    setFixedSize(560, 640);
    setWindowTitle("Chat Client");

    // 绑定信号和槽
    connect(_loginDialog, &LoginDialog::switchRegister, this, [this]() {
        _authStack->setCurrentIndex(1);
        setFixedSize(560, 640);
    });
    connect(_registerDialog, &RegisterDialog::switchToLogin, this, [this]() {
        _authStack->setCurrentIndex(0);
        setFixedSize(560, 640);
    });

    connect(_loginDialog, &LoginDialog::switchReset, this, [this]() {
        _authStack->setCurrentIndex(2);
        setFixedSize(560, 640);
    });
    connect(_resetDialog, &ResetDialog::switchToLogin, this, [this]() {
        _authStack->setCurrentIndex(0);
        setFixedSize(560, 640);
    });

    connect(&TcpMgr::getInstance(), &TcpMgr::sig_switch_chatdlg, this, [this]() {
        if (!LocalDb::instance().switchUser(UserMgr::getInstance().GetUid())) {
            QMessageBox::critical(this,
                                  QStringLiteral("本地数据库初始化失败"),
                                  QStringLiteral("无法切换到当前用户的本地数据库：\n%1").arg(LocalDb::instance().lastError()));
            return;
        }
        _chatPage->setCurrentUser(UserMgr::getInstance().GetUid(),
                                  UserMgr::getInstance().GetName());
        setWindowTitle(UserMgr::getInstance().GetName());
        setMinimumSize(880, 590);
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        resize(1110, 730);
        _contentStack->setCurrentWidget(_chatPage);
    });

    connect(&TcpMgr::getInstance(), &TcpMgr::sig_server_closed, this, [this]() {
        QMessageBox::warning(this, "连接断开", "聊天服务器已关闭或连接已断开，请重新登录。", QMessageBox::Ok);
        _friendRequestFlashActive = false;
        _chatMessageFlashActive = false;
        updateTaskbarFlashState();
        setWindowTitle("Chat Client");
        _authStack->setCurrentIndex(0);
        setFixedSize(560, 640);
        _contentStack->setCurrentWidget(_authContainer);
    });

    connect(_chatPage, &ChatPage::friendRequestNotificationChanged, this, [this](bool hasUnread) {
        _friendRequestFlashActive = hasUnread;
        updateTaskbarFlashState();
    });

    connect(_chatPage, &ChatPage::chatMessageNotificationChanged, this, [this](bool hasUnread) {
        _chatMessageFlashActive = hasUnread;
        updateTaskbarFlashState();
    });
}

MainWindow::~MainWindow()
{
    delete ui;

}

bool MainWindow::event(QEvent *event)
{
    if (event->type() == QEvent::WindowActivate && (_friendRequestFlashActive || _chatMessageFlashActive)) {
        stopTaskbarFlash();
    }
    return QMainWindow::event(event);
}

void MainWindow::updateTaskbarFlashState()
{
    const bool enabled = _friendRequestFlashActive || _chatMessageFlashActive;
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

void MainWindow::stopTaskbarFlash()
{
#ifdef Q_OS_WIN
    FLASHWINFO info;
    info.cbSize = sizeof(FLASHWINFO);
    info.hwnd = reinterpret_cast<HWND>(winId());
    info.dwFlags = FLASHW_STOP;
    info.uCount = 0;
    info.dwTimeout = 0;
    FlashWindowEx(&info);
#endif
}
