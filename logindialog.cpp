#include "logindialog.h"
#include "ui_logindialog.h"
#include <QMessageBox>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>
#include "httpmgr.h"
#include "TcpMgr.h"

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
    setWindowTitle(QString::fromUtf8(u8"登录"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(
        "QDialog { background:#ffffff; border-radius:24px; }"
        "QLabel#label { font: 700 24px 'Microsoft YaHei UI'; color:#111827; }"
        "QLabel#subtitleLabel, QLabel#registerHintLabel { font: 10pt 'Microsoft YaHei UI'; color:#6b7280; background:transparent; }"
        "QLabel#userLabel, QLabel#pswdLabel { font: 10pt 'Microsoft YaHei UI'; color:#374151; min-width:56px; background:transparent; }"
        "QLineEdit { background:#ffffff; border:1px solid #ddd6e8; border-radius:14px; min-height:40px; padding:0 12px; font: 10pt 'Microsoft YaHei UI'; color:#111827; }"
        "QLineEdit:focus { border:1px solid #b7b2c7; background:#ffffff; }"
        "QPushButton { min-height:40px; border:none; border-radius:14px; font: 10pt 'Microsoft YaHei UI'; padding:0 18px; }"
        "QPushButton#loginBtn { background:#111827; color:white; }"
        "QPushButton#loginBtn:pressed { background:#1f2937; }"
        "QPushButton#regButton, QPushButton#forgetBtn { background:#ffffff; color:#374151; border:1px solid #ddd6e8; }"
        "QPushButton#regButton:pressed, QPushButton#forgetBtn:pressed { background:#f7f5fb; }");
    ui->label->setText(QString::fromUtf8(u8"欢迎回来"));
    ui->userLabel->setText(QString::fromUtf8(u8"邮箱"));
    ui->pswdLabel->setText(QString::fromUtf8(u8"密码"));
    ui->regButton->setText(QString::fromUtf8(u8"去注册"));
    ui->loginBtn->setText(QString::fromUtf8(u8"登录"));
    ui->forgetBtn->setText(QString::fromUtf8(u8"忘记密码"));
    ui->emailLineEdit->setPlaceholderText(QString::fromUtf8(u8"请输入邮箱地址"));
    ui->pswdLineEdit->setPlaceholderText(QString::fromUtf8(u8"请输入密码"));

    // 绑定信号与槽
    connect(ui->regButton, &QPushButton::clicked, this, [this]() {
        emit switchRegister(); // 发出切换到注册界面的信号
    });
    connect(ui->forgetBtn, &QPushButton::clicked, this, [this]() {
        emit switchReset();    // 发出切换到重置界面的信号
    });

    initHttpHandlers();
    connect(&HttpMgr::getInstance(), &HttpMgr::sig_login_mod_http_finished,
            this, &LoginDialog::slot_login_mod_http_finished);
    // 连接 tcp 连接请求的信号和槽函数
    connect(this, &LoginDialog::sig_connect_tcp, &TcpMgr::getInstance(), &TcpMgr::slot_tcp_connect);
    // 连接 tcp 管理者发出的连接成功信号
    connect(&TcpMgr::getInstance(), &TcpMgr::sig_con_success, this, &LoginDialog::slot_tcp_con_finish);
    // 连接 tcp 管理者发出的登录失败信号
    connect(&TcpMgr::getInstance(), &TcpMgr::sig_login_failed, this, &LoginDialog::slot_login_failed);
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

void LoginDialog::on_loginBtn_clicked()
{
    if (_loginRequestInFlight) {
        return;
    }

    QString email = ui->emailLineEdit->text().trimmed();
    QString password = ui->pswdLineEdit->text().trimmed();

    // 快速非空检查
    if (email.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "登录失败", "请填写所有必填项！");
        return;
    }

    QJsonObject json_obj;
    json_obj["email"] = email;
    json_obj["passwd"] = password;
    _loginRequestInFlight = true;
    ui->loginBtn->setEnabled(false);
    HttpMgr::getInstance().PostHttpReq(QUrl(gate_url_prefix + "/user_login"),
                                       json_obj, ReqId::ID_LOGIN_USER, Modules::LOGINMOD);
}

void LoginDialog::slot_login_mod_http_finished(ReqId id, QString res, ErrorCodes err)
{
    if (err != ErrorCodes::SUCCESS) {
        if (id == ReqId::ID_LOGIN_USER) {
            _loginRequestInFlight = false;
            ui->loginBtn->setEnabled(true);
        }
        QMessageBox::warning(this, "错误", "网络请求失败，请重试");
        return;
    }

    // 解析 JSON 字符串，转化为 QByteArray
    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    if (jsonDoc.isNull()) {
        if (id == ReqId::ID_LOGIN_USER) {
            _loginRequestInFlight = false;
            ui->loginBtn->setEnabled(true);
        }
        qDebug() << "[LoginDialog.cpp] 函数 [slot_login_mod_http_finished] JSON 解析失败: " << res;
        return;
    }
    if (!jsonDoc.isObject()) {
        if (id == ReqId::ID_LOGIN_USER) {
            _loginRequestInFlight = false;
            ui->loginBtn->setEnabled(true);
        }
        qDebug() << "[LoginDialog.cpp] 函数 [slot_login_mod_http_finished] JSON 不是对象: " << res;
        return;
    }
    _handlers[id](jsonDoc.object());
}

void LoginDialog::initHttpHandlers()
{
    _handlers[ReqId::ID_LOGIN_USER] = [this](const QJsonObject& jsonObj) {
        int error = jsonObj.value("error").toInt();
        if (error != ErrorCodes::SUCCESS) {
            _loginRequestInFlight = false;
            ui->loginBtn->setEnabled(true);
            qDebug() << "[LoginDialog.cpp] 函数 [initHttpHandlers] 登录失败: " << jsonObj;
            QString errorMessage = jsonObj.value("message").toString();
            if (errorMessage.isEmpty()) {
                errorMessage = QStringLiteral("用户名或密码错误，请重试");
            }
            QMessageBox::warning(this, QStringLiteral("登录失败"), errorMessage);
            return;
        }
        auto user = jsonObj["user"].toString();

        // 发送信号通知 tcpMgr 发起长链接
        ServerInfo si;
        si.Uid = jsonObj["uid"].toInt();
        si.Host = jsonObj["host"].toString();
        si.Port = jsonObj["port"].toString();
        si.Token = jsonObj["token"].toString();

        _uid = si.Uid;
        _token = si.Token;
        qDebug() << "user is " << user << " uid is " << si.Uid << " host is "
                 << si.Host << " Port is " << si.Port << " Token is " << si.Token;
        emit sig_connect_tcp(si);
        // QMessageBox::information(this, "成功", "登录成功");
    };
}

void LoginDialog::slot_tcp_con_finish(bool bsuccess)
{
    if (bsuccess) {
        QJsonObject jsonObj;
        jsonObj["uid"] = _uid;
        jsonObj["token"] = _token;

        QJsonDocument doc(jsonObj);
        QString jsonString = doc.toJson(QJsonDocument::Indented);

        // 发送 tcp 请求给 ChatServer
        emit TcpMgr::getInstance().sig_send_data(ReqId::ID_CHAT_LOGIN, jsonString);
    }
    else {
        _loginRequestInFlight = false;
        ui->loginBtn->setEnabled(true);
        QMessageBox::warning(this, "错误", "TCP 长链接请求失败，请重试");
    }
}

void LoginDialog::slot_login_failed(int err)
{
    _loginRequestInFlight = false;
    ui->loginBtn->setEnabled(true);
    QMessageBox::warning(this, "登录失败", "登录失败，错误码: " + QString::number(err));
}
