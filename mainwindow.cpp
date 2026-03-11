#include "mainwindow.h"
#include "ui_mainwindow.h"

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
    _stackedWidget->setCurrentWidget(_chatPage);

    resize(1110, 770);
    setMinimumSize(880, 610);
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
}

MainWindow::~MainWindow()
{
    delete ui;

}
