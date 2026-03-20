#include "resetdialog.h"
#include "ui_resetdialog.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QRegularExpression>

#include "httpmgr.h"

ResetDialog::ResetDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ResetDialog)
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("重置密码"));
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

    ui->hintLabel->setText(QStringLiteral("找回密码"));
    ui->userLabel->setText(QStringLiteral("用户名"));
    ui->emailLabel->setText(QStringLiteral("邮箱"));
    ui->codeLabel->setText(QStringLiteral("验证码"));
    ui->passLabel->setText(QStringLiteral("新密码"));
    ui->getCodeBtn->setText(QStringLiteral("获取验证码"));
    ui->cancelBtn->setText(QStringLiteral("返回登录"));
    ui->resetBtn->setText(QStringLiteral("重置密码"));
    ui->userLineEdit->setPlaceholderText(QStringLiteral("请输入用户名"));
    ui->emailLineEdit->setPlaceholderText(QStringLiteral("请输入邮箱地址"));
    ui->codeLineEdit->setPlaceholderText(QStringLiteral("请输入验证码"));
    ui->passLineEdit->setPlaceholderText(QStringLiteral("请输入新的登录密码"));
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
    _handlers[ReqId::ID_GET_VERIFY_CODE] = [this](const QJsonObject &jsonObj) {
        const int error = jsonObj.value("error").toInt();
        if (error != ErrorCodes::SUCCESS) {
            qDebug() << "[ResetDialog.cpp] [initHttpHandlers] 获取验证码失败:" << jsonObj;
            startVerifyCountdown(0);
            QMessageBox::warning(this,
                                 QStringLiteral("错误"),
                                 QStringLiteral("获取验证码失败，请重试"));
            return;
        }

        const QString email = jsonObj.value("email").toString();
        QMessageBox::information(this,
                                 QStringLiteral("成功"),
                                 QStringLiteral("验证码已发送到 ") + email);
    };

    _handlers[ReqId::ID_RESET_PWD] = [this](const QJsonObject &jsonObj) {
        const int error = jsonObj.value("error").toInt();
        if (error != ErrorCodes::SUCCESS) {
            qDebug() << "[ResetDialog.cpp] [initHttpHandlers] 重置密码失败:" << jsonObj;
            QMessageBox::warning(this,
                                 QStringLiteral("重置失败"),
                                 QStringLiteral("重置密码失败，请重试"));
            return;
        }

        QMessageBox::information(this,
                                 QStringLiteral("成功"),
                                 QStringLiteral("密码重置成功，请重新登录"));
        emit switchToLogin();
    };
}

void ResetDialog::on_getCodeBtn_clicked()
{
    if (_verifyCountdownRemaining > 0) {
        return;
    }

    const QString email = ui->emailLineEdit->text().trimmed();
    const QRegularExpression emailRegex(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
    if (!emailRegex.match(email).hasMatch()) {
        QMessageBox::warning(this,
                             QStringLiteral("错误"),
                             QStringLiteral("邮箱格式不正确"));
        return;
    }

    startVerifyCountdown();
    QJsonObject jsonObj;
    jsonObj["email"] = email;
    HttpMgr::getInstance().PostHttpReq(
        QUrl(gate_url_prefix + "/get_verifycode"),
        jsonObj,
        ReqId::ID_GET_VERIFY_CODE,
        Modules::RESETMOD);
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
        ui->getCodeBtn->setText(QStringLiteral("%1秒后重发").arg(_verifyCountdownRemaining));
    } else {
        ui->getCodeBtn->setText(QStringLiteral("获取验证码"));
    }
}

void ResetDialog::on_cancelBtn_clicked()
{
    emit switchToLogin();
}

void ResetDialog::on_resetBtn_clicked()
{
    const QString username = ui->userLineEdit->text().trimmed();
    const QString email = ui->emailLineEdit->text().trimmed();
    const QString verifyCode = ui->codeLineEdit->text().trimmed();
    const QString password = ui->passLineEdit->text();

    if (username.isEmpty() || email.isEmpty() || verifyCode.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this,
                             QStringLiteral("重置失败"),
                             QStringLiteral("请填写所有必填项。"));
        return;
    }

    QJsonObject jsonObj;
    jsonObj["user"] = username;
    jsonObj["email"] = email;
    jsonObj["verifycode"] = verifyCode;
    jsonObj["passwd"] = password;

    HttpMgr::getInstance().PostHttpReq(
        QUrl(gate_url_prefix + "/reset_pwd"),
        jsonObj,
        ReqId::ID_RESET_PWD,
        Modules::RESETMOD);
}

void ResetDialog::slot_reset_mod_http_finished(ReqId id, QString res, ErrorCodes err)
{
    if (err != ErrorCodes::SUCCESS) {
        if (id == ReqId::ID_GET_VERIFY_CODE) {
            startVerifyCountdown(0);
        }
        QMessageBox::warning(this,
                             QStringLiteral("错误"),
                             QStringLiteral("网络请求失败，请重试"));
        return;
    }

    const QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    if (jsonDoc.isNull()) {
        qDebug() << "[ResetDialog.cpp] [slot_reset_mod_http_finished] JSON 解析失败:" << res;
        return;
    }
    if (!jsonDoc.isObject()) {
        qDebug() << "[ResetDialog.cpp] [slot_reset_mod_http_finished] JSON 不是对象:" << res;
        return;
    }

    if (_handlers.contains(id)) {
        _handlers[id](jsonDoc.object());
    }
}
