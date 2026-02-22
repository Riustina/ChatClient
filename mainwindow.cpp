#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    _stackedWidget = new QStackedWidget(this);
    setCentralWidget(_stackedWidget);

    _loginDialog = new LoginDialog(this);
    _registerDialog = new RegisterDialog(this);
    _stackedWidget->addWidget(_loginDialog);
    _stackedWidget->addWidget(_registerDialog);
    _stackedWidget->setCurrentWidget(_loginDialog);

    // 绑定信号和槽
    connect(_loginDialog, &LoginDialog::switchRegister, this, [this]() {
        _stackedWidget->setCurrentWidget(_registerDialog);
    });

}

MainWindow::~MainWindow()
{
    delete ui;

}
