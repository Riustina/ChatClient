#include "logindialog.h"
#include "ui_logindialog.h"

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
{
    ui->setupUi(this);

    // 绑定信号与槽
    connect(ui->regButton, &QPushButton::clicked, this, [this]() {
        emit switchRegister(); // 发出切换到注册界面的信号
    });
}

LoginDialog::~LoginDialog()
{
    delete ui;
}
