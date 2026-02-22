#include "registerdialog.h"
#include "ui_registerdialog.h"
#include <QRegularExpression>
#include <QMessageBox>

RegisterDialog::RegisterDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RegisterDialog)
{
    ui->setupUi(this);
}

RegisterDialog::~RegisterDialog()
{
    delete ui;
}

void RegisterDialog::on_sendCodeButton_clicked()
{
    // 验证emailLineEdit中的邮箱地址合法性
    QString email = ui->emailLineEdit->text();
    QRegularExpression emailRegex(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
    if (!emailRegex.match(email).hasMatch()) {
        QMessageBox::warning(this, "错误", "邮箱格式不正确");
        return;
    } else {
        // 发送验证码
        QMessageBox::information(this, "成功", "验证码已发送到您的邮箱");
    }
}

