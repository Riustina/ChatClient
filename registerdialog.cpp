#include "registerdialog.h"
#include "ui_registerdialog.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QRegularExpression>

#include "httpmgr.h"

RegisterDialog::RegisterDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RegisterDialog)
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("注册"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(
        "QDialog { background:#ffffff; border-radius:24px; }"
        "QLabel#label { font: 700 24px 'Microsoft YaHei UI'; color:#111827; }"
        "QLabel#subtitleLabel { font: 10pt 'Microsoft YaHei UI'; color:#6b7280; background:transparent; }"
        "QLabel#userLabel, QLabel#emailLabel, QLabel#codeLabel, QLabel#pswdLabel { font: 10pt 'Microsoft YaHei UI'; color:#374151; min-width:72px; background:transparent; }"
        "QLineEdit { background:#ffffff; border:1px solid #ddd6e8; border-radius:14px; min-height:40px; padding:0 12px; font: 10pt 'Microsoft YaHei UI'; color:#111827; }"
        "QLineEdit:focus { border:1px solid #b7b2c7; background:#ffffff; }"
        "QPushButton { min-height:40px; border:none; border-radius:14px; font: 10pt 'Microsoft YaHei UI'; padding:0 18px; }"
        "QPushButton#registerButton { background:#111827; color:white; }"
        "QPushButton#registerButton:pressed { background:#1f2937; }"
        "QPushButton#sendCodeButton, QPushButton#cancelButton { background:#ffffff; color:#374151; border:1px solid #ddd6e8; }"
        "QPushButton#sendCodeButton:pressed, QPushButton#cancelButton:pressed { background:#f7f5fb; }");

    ui->label->setText(QStringLiteral("创建账号"));
    ui->userLabel->setText(QStringLiteral("用户名"));
    ui->emailLabel->setText(QStringLiteral("邮箱"));
    ui->codeLabel->setText(QStringLiteral("验证码"));
    ui->pswdLabel->setText(QStringLiteral("密码"));
    ui->sendCodeButton->setText(QStringLiteral("获取验证码"));
    ui->cancelButton->setText(QStringLiteral("返回登录"));
    ui->registerButton->setText(QStringLiteral("注册"));
    ui->userLineEdit->setPlaceholderText(QStringLiteral("请输入用户名"));
    ui->emailLineEdit->setPlaceholderText(QStringLiteral("请输入邮箱地址"));
    ui->codeLineEdit->setPlaceholderText(QStringLiteral("请输入验证码"));
    ui->pswdLineEdit->setPlaceholderText(QStringLiteral("请设置登录密码"));

    initHttpHandlers();

    _verifyCountdownTimer.setInterval(1000);
    connect(&_verifyCountdownTimer, &QTimer::timeout, this, [this]() {
        if (_verifyCountdownRemaining > 0) {
            --_verifyCountdownRemaining;
        }
        if (_verifyCountdownRemaining <= 0) {
            _verifyCountdownTimer.stop();
            ui->sendCodeButton->setEnabled(true);
        }
        updateVerifyButtonText();
    });
    updateVerifyButtonText();

    connect(&HttpMgr::getInstance(), &HttpMgr::sig_reg_mod_http_finished,
            this, &RegisterDialog::slot_reg_mod_http_finished);
}

RegisterDialog::~RegisterDialog()
{
    delete ui;
}

void RegisterDialog::initHttpHandlers()
{
    _handlers[ReqId::ID_GET_VERIFY_CODE] = [this](const QJsonObject &jsonObj) {
        const int error = jsonObj.value("error").toInt();
        if (error != ErrorCodes::SUCCESS) {
            qDebug() << "[RegisterDialog.cpp] [initHttpHandlers] 获取验证码失败:" << jsonObj;
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

    _handlers[ReqId::ID_REG_USER] = [this](const QJsonObject &jsonObj) {
        const int error = jsonObj.value("error").toInt();
        if (error != ErrorCodes::SUCCESS) {
            qDebug() << "[RegisterDialog.cpp] [initHttpHandlers] 注册失败:" << jsonObj;
            QMessageBox::warning(this,
                                 QStringLiteral("注册失败"),
                                 QStringLiteral("注册失败，请重试"));
            return;
        }

        QMessageBox::information(this,
                                 QStringLiteral("成功"),
                                 QStringLiteral("注册成功，请登录"));
        emit switchToLogin();
    };
}

void RegisterDialog::on_sendCodeButton_clicked()
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
    HttpMgr::getInstance().PostHttpReq(QUrl(gate_url_prefix + "/get_verifycode"),
                                       jsonObj, ReqId::ID_GET_VERIFY_CODE, Modules::REGISTERMOD);
}

void RegisterDialog::startVerifyCountdown(int seconds)
{
    _verifyCountdownRemaining = qMax(0, seconds);
    ui->sendCodeButton->setEnabled(_verifyCountdownRemaining <= 0);
    updateVerifyButtonText();
    if (_verifyCountdownRemaining > 0) {
        _verifyCountdownTimer.start();
    } else {
        _verifyCountdownTimer.stop();
    }
}

void RegisterDialog::updateVerifyButtonText()
{
    if (_verifyCountdownRemaining > 0) {
        ui->sendCodeButton->setText(QStringLiteral("%1秒后重发").arg(_verifyCountdownRemaining));
    } else {
        ui->sendCodeButton->setText(QStringLiteral("获取验证码"));
    }
}

void RegisterDialog::slot_reg_mod_http_finished(ReqId id, QString res, ErrorCodes err)
{
    if (err != ErrorCodes::SUCCESS) {
        if (id == ReqId::ID_GET_VERIFY_CODE) {
            startVerifyCountdown(0);
            QMessageBox::warning(this,
                                 QStringLiteral("错误"),
                                 QStringLiteral("获取验证码失败，请重试"));
        } else {
            QMessageBox::warning(this,
                                 QStringLiteral("错误"),
                                 QStringLiteral("注册失败，请重试"));
        }
        return;
    }

    const QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    if (jsonDoc.isNull()) {
        qDebug() << "[RegisterDialog.cpp] [slot_reg_mod_http_finished] JSON 解析失败:" << res;
        return;
    }
    if (!jsonDoc.isObject()) {
        qDebug() << "[RegisterDialog.cpp] [slot_reg_mod_http_finished] JSON 不是对象:" << res;
        return;
    }

    if (_handlers.contains(id)) {
        _handlers[id](jsonDoc.object());
    }
}

void RegisterDialog::on_cancelButton_clicked()
{
    emit switchToLogin();
}

void RegisterDialog::on_registerButton_clicked()
{
    const QString username = ui->userLineEdit->text().trimmed();
    const QString email = ui->emailLineEdit->text().trimmed();
    const QString verifyCode = ui->codeLineEdit->text().trimmed();
    const QString password = ui->pswdLineEdit->text();

    if (username.isEmpty() || email.isEmpty() || verifyCode.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this,
                             QStringLiteral("注册失败"),
                             QStringLiteral("请填写所有必填项。"));
        return;
    }

    QJsonObject jsonObj;
    jsonObj["user"] = username;
    jsonObj["email"] = email;
    jsonObj["verifycode"] = verifyCode;
    jsonObj["passwd"] = password;

    HttpMgr::getInstance().PostHttpReq(
        QUrl(gate_url_prefix + "/user_register"),
        jsonObj,
        ReqId::ID_REG_USER,
        Modules::REGISTERMOD);
}
