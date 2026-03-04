#include "resetdialog.h"
#include "ui_resetdialog.h"
#include <QRegularExpression>
#include <QMessageBox>
#include <QJsonObject>
#include "httpmgr.h"

ResetDialog::ResetDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ResetDialog)
{
    ui->setupUi(this);

    initHttpHandlers();

    connect(&HttpMgr::getInstance(), &HttpMgr::sig_reset_mod_http_finished,
            this, &ResetDialog::slot_reset_mod_http_finished);
}

ResetDialog::~ResetDialog()
{
    delete ui;
}

void ResetDialog::initHttpHandlers()
{
    // 获取验证码回包
    _handlers[ReqId::ID_GET_VERIFY_CODE] = [this](const QJsonObject& jsonObj) {
        int error = jsonObj.value("error").toInt();
        if (error != ErrorCodes::SUCCESS) {
            qDebug() << "[ResetDialog.cpp] 函数 [initHttpHandlers] 获取验证码失败: " << jsonObj;
            QMessageBox::warning(this, "错误", "获取验证码失败，请重试");
            return;
        }

        auto email = jsonObj.value("email").toString();
        QMessageBox::information(this, "成功", "验证码已发送到 " + email);
    };

    // 重置密码回包
    _handlers[ReqId::ID_RESET_PWD] = [this](const QJsonObject& jsonObj) {
        int error = jsonObj.value("error").toInt();
        if (error != ErrorCodes::SUCCESS) {
            qDebug() << "[ResetDialog.cpp] 函数 [initHttpHandlers] 重置密码失败: " << jsonObj;
            QMessageBox::warning(this, "重置失败", "重置密码失败，请重试");
            return;
        }

        QMessageBox::information(this, "成功", "密码重置成功，请重新登录");
        emit switchToLogin();
    };
}

void ResetDialog::on_getCodeBtn_clicked()
{
    QString email = ui->emailLineEdit->text().trimmed();
    QRegularExpression emailRegex(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
    if (!emailRegex.match(email).hasMatch()) {
        QMessageBox::warning(this, "错误", "邮箱格式不正确");
        return;
    }

    QJsonObject jsonObj;
    jsonObj["email"] = email;
    HttpMgr::getInstance().PostHttpReq(
        QUrl(gate_url_prefix + "/get_verifycode"),
        jsonObj,
        ReqId::ID_GET_VERIFY_CODE,
        Modules::RESETMOD
        );
}

void ResetDialog::on_cancelBtn_clicked()
{
    emit switchToLogin();
}

void ResetDialog::on_resetBtn_clicked()
{
    QString username   = ui->userLineEdit->text().trimmed();
    QString email      = ui->emailLineEdit->text().trimmed();
    QString verifyCode = ui->codeLineEdit->text().trimmed();
    QString password   = ui->passLineEdit->text();

    if (username.isEmpty() || email.isEmpty() ||
        verifyCode.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "重置失败", "请填写所有必填项！");
        return;
    }

    QJsonObject jsonObj;
    jsonObj["user"]       = username;
    jsonObj["email"]      = email;
    jsonObj["verifycode"] = verifyCode;
    jsonObj["passwd"]     = password;

    HttpMgr::getInstance().PostHttpReq(
        QUrl(gate_url_prefix + "/reset_pwd"),
        jsonObj,
        ReqId::ID_RESET_PWD,
        Modules::RESETMOD
        );
}

void ResetDialog::slot_reset_mod_http_finished(ReqId id, QString res, ErrorCodes err)
{
    if (err != ErrorCodes::SUCCESS) {
        QMessageBox::warning(this, "错误", "网络请求失败，请重试");
        return;
    }

    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    if (jsonDoc.isNull()) {
        qDebug() << "[ResetDialog.cpp] 函数 [slot_reset_mod_http_finished] JSON 解析失败: " << res;
        return;
    }

    if (!jsonDoc.isObject()) {
        qDebug() << "[ResetDialog.cpp] 函数 [slot_reset_mod_http_finished] JSON 不是对象: " << res;
        return;
    }

    _handlers[id](jsonDoc.object());
}
