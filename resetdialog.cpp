#include "resetdialog.h"
#include "ui_resetdialog.h"
#include <QRegularExpression>
#include <QMessageBox>
#include <QJsonObject>
#include <QLineEdit>
#include "httpmgr.h"

ResetDialog::ResetDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ResetDialog)
{
    ui->setupUi(this);
    setWindowTitle(QString::fromUtf8(u8"重置密码"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(
        "QDialog { background:#ffffff; border-radius:24px; }"
        "QLabel#hintLabel { font: 700 24px 'Microsoft YaHei UI'; color:#111827; }"
        "QLabel#subtitleLabel { font: 10pt 'Microsoft YaHei UI'; color:#6b7280; background:transparent; }"
        "QLabel#userLabel, QLabel#emailLabel, QLabel#codeLabel, QLabel#passLabel { font: 10pt 'Microsoft YaHei UI'; color:#374151; min-width:72px; background:transparent; }"
        "QLineEdit { background:#ffffff; border:1px solid #ddd6e8; border-radius:14px; min-height:40px; padding:0 12px; font: 10pt 'Microsoft YaHei UI'; color:#111827; }"
        "QLineEdit:focus { border:1px solid #b7b2c7; background:#ffffff; }"
        "QPushButton { min-height:40px; border:none; border-radius:14px; font: 10pt 'Microsoft YaHei UI'; padding:0 18px; }"
        "QPushButton#resetBtn { background:#111827; color:white; }"
        "QPushButton#resetBtn:pressed { background:#1f2937; }"
        "QPushButton#getCodeBtn, QPushButton#cancelBtn { background:#ffffff; color:#374151; border:1px solid #ddd6e8; }"
        "QPushButton#getCodeBtn:pressed, QPushButton#cancelBtn:pressed { background:#f7f5fb; }");
    ui->hintLabel->setText(QString::fromUtf8(u8"找回密码"));
    ui->userLabel->setText(QString::fromUtf8(u8"用户名"));
    ui->emailLabel->setText(QString::fromUtf8(u8"邮箱"));
    ui->codeLabel->setText(QString::fromUtf8(u8"验证码"));
    ui->passLabel->setText(QString::fromUtf8(u8"新密码"));
    ui->getCodeBtn->setText(QString::fromUtf8(u8"获取验证码"));
    ui->cancelBtn->setText(QString::fromUtf8(u8"返回登录"));
    ui->resetBtn->setText(QString::fromUtf8(u8"重置密码"));
    ui->userLineEdit->setPlaceholderText(QString::fromUtf8(u8"请输入用户名"));
    ui->emailLineEdit->setPlaceholderText(QString::fromUtf8(u8"请输入邮箱地址"));
    ui->codeLineEdit->setPlaceholderText(QString::fromUtf8(u8"请输入验证码"));
    ui->passLineEdit->setPlaceholderText(QString::fromUtf8(u8"请输入新的登录密码"));
    ui->passLineEdit->setEchoMode(QLineEdit::Password);

    initHttpHandlers();

    _verifyCountdownTimer.setInterval(1000);
    connect(&_verifyCountdownTimer, &QTimer::timeout, this, [this]() {
        if (_verifyCountdownRemaining > 0) {
            --_verifyCountdownRemaining;
        }
        if (_verifyCountdownRemaining <= 0) {
            _verifyCountdownTimer.stop();
            ui->getCodeBtn->setEnabled(true);
        }
        updateVerifyButtonText();
    });
    updateVerifyButtonText();

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
            startVerifyCountdown(0);
            QMessageBox::warning(this,
                                 QString::fromUtf8(u8"\u9519\u8bef"),
                                 QString::fromUtf8(u8"\u83b7\u53d6\u9a8c\u8bc1\u7801\u5931\u8d25\uff0c\u8bf7\u91cd\u8bd5"));
            return;
        }

        auto email = jsonObj.value("email").toString();
        QMessageBox::information(this,
                                 QString::fromUtf8(u8"\u6210\u529f"),
                                 QString::fromUtf8(u8"\u9a8c\u8bc1\u7801\u5df2\u53d1\u9001\u5230 ") + email);
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
    if (_verifyCountdownRemaining > 0) {
        return;
    }

    QString email = ui->emailLineEdit->text().trimmed();
    QRegularExpression emailRegex(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
    if (!emailRegex.match(email).hasMatch()) {
        QMessageBox::warning(this,
                             QString::fromUtf8(u8"\u9519\u8bef"),
                             QString::fromUtf8(u8"\u90ae\u7bb1\u683c\u5f0f\u4e0d\u6b63\u786e"));
        return;
    }

    startVerifyCountdown();
    QJsonObject jsonObj;
    jsonObj["email"] = email;
    HttpMgr::getInstance().PostHttpReq(
        QUrl(gate_url_prefix + "/get_verifycode"),
        jsonObj,
        ReqId::ID_GET_VERIFY_CODE,
        Modules::RESETMOD
        );
}

void ResetDialog::startVerifyCountdown(int seconds)
{
    _verifyCountdownRemaining = qMax(0, seconds);
    ui->getCodeBtn->setEnabled(_verifyCountdownRemaining <= 0);
    updateVerifyButtonText();
    if (_verifyCountdownRemaining > 0) {
        _verifyCountdownTimer.start();
    } else {
        _verifyCountdownTimer.stop();
    }
}

void ResetDialog::updateVerifyButtonText()
{
    if (_verifyCountdownRemaining > 0) {
        ui->getCodeBtn->setText(QString::fromUtf8(u8"%1\u79d2\u540e\u91cd\u53d1").arg(_verifyCountdownRemaining));
    } else {
        ui->getCodeBtn->setText(QString::fromUtf8(u8"\u83b7\u53d6\u9a8c\u8bc1\u7801"));
    }
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
        if (id == ReqId::ID_GET_VERIFY_CODE) {
            startVerifyCountdown(0);
        }
        QMessageBox::warning(this,
                             QString::fromUtf8(u8"\u9519\u8bef"),
                             QString::fromUtf8(u8"\u7f51\u7edc\u8bf7\u6c42\u5931\u8d25\uff0c\u8bf7\u91cd\u8bd5"));
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
