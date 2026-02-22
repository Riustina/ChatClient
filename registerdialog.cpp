#include "registerdialog.h"
#include "ui_registerdialog.h"
#include <QRegularExpression>
#include <QMessageBox>
#include "httpmgr.h"

RegisterDialog::RegisterDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RegisterDialog)
{
    ui->setupUi(this);

    initHttpHandlers();

    connect(&HttpMgr::getInstance(), &HttpMgr::sig_reg_mod_http_finished, this, &RegisterDialog::slot_reg_mod_http_finished);
}

RegisterDialog::~RegisterDialog()
{
    delete ui;
}

void RegisterDialog::initHttpHandlers()
{
    // 注册获取验证码回包的逻辑
    _handlers[ReqId::ID_GET_VERIFY_CODE] = [this](const QJsonObject& jsonObj) {
        int error = jsonObj.value("error").toInt();
        if (error != ErrorCodes::SUCCESS) {
            qDebug() << "[RegisterDialog.cpp] 函数 [initHttpHandlers] 获取验证码失败: " << jsonObj;
            return;
        }

        auto email = jsonObj.value("email").toString();
        QMessageBox::information(this, "成功", "验证码已发送到 " + email);
    };
}

void RegisterDialog::on_sendCodeButton_clicked()
{
    // 验证emailLineEdit中的邮箱地址合法性
    QString email = ui->emailLineEdit->text();
    QRegularExpression emailRegex(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
    if (!emailRegex.match(email).hasMatch()) {
        QMessageBox::warning(this, "错误", "邮箱格式不正确");
        return;
    }
    // else {
        // 发送验证码
        // QMessageBox::information(this, "成功", "验证码已发送到您的邮箱");
    // }
}

void RegisterDialog::slot_reg_mod_http_finished(ReqId id, QString res, ErrorCodes err)
{
    if (err != ErrorCodes::SUCCESS) {
        QMessageBox::warning(this, "错误", "注册失败，请重试");
        return;
    }

    // 解析 JSON 字符串，将 res 转化为 QByteArray
    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());  // 将 QString 转换为 QByteArray
    if (jsonDoc.isNull()) {
        qDebug() << "[RegisterDialog.cpp] 函数 [slot_reg_mod_http_finished] JSON 解析失败: " << res;
        return;
    }

    // 检查 JSON 是否为对象类型
    if (!jsonDoc.isObject()) {
        qDebug() << "[RegisterDialog.cpp] 函数 [slot_reg_mod_http_finished] JSON 不是对象: " << res;
        return;
    }

    // 根据请求 ID 调用对应的处理函数
    _handlers[id](jsonDoc.object());
}
