#include "logindialog.h"
#include "ui_logindialog.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>

#include "httpmgr.h"
#include "TcpMgr.h"

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
    setWindowTitle(QString::fromUtf8(u8"\u767b\u5f55"));
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

    ui->label->setText(QString::fromUtf8(u8"\u6b22\u8fce\u56de\u6765"));
    ui->userLabel->setText(QString::fromUtf8(u8"\u90ae\u7bb1"));
    ui->pswdLabel->setText(QString::fromUtf8(u8"\u5bc6\u7801"));
    ui->regButton->setText(QString::fromUtf8(u8"\u53bb\u6ce8\u518c"));
    ui->loginBtn->setText(QString::fromUtf8(u8"\u767b\u5f55"));
    ui->forgetBtn->setText(QString::fromUtf8(u8"\u5fd8\u8bb0\u5bc6\u7801"));
    ui->emailLineEdit->setPlaceholderText(QString::fromUtf8(u8"\u8bf7\u8f93\u5165\u90ae\u7bb1\u5730\u5740"));
    ui->pswdLineEdit->setPlaceholderText(QString::fromUtf8(u8"\u8bf7\u8f93\u5165\u5bc6\u7801"));

    _rememberPasswordCheckBox = new QCheckBox(QString::fromUtf8(u8"\u8bb0\u4f4f\u5bc6\u7801"), this);
    _rememberPasswordCheckBox->setObjectName(QStringLiteral("rememberPasswordCheckBox"));
    _rememberPasswordCheckBox->setStyleSheet(
        "QCheckBox { font: 10pt 'Microsoft YaHei UI'; color:#4b5563; spacing:6px; }"
        "QCheckBox::indicator { width:16px; height:16px; }");
    if (ui->helperLayout != nullptr) {
        ui->helperLayout->insertWidget(0, _rememberPasswordCheckBox);
    }

    loadRememberedCredentials();

    connect(ui->regButton, &QPushButton::clicked, this, [this]() {
        emit switchRegister();
    });
    connect(ui->forgetBtn, &QPushButton::clicked, this, [this]() {
        emit switchReset();
    });

    initHttpHandlers();
    connect(&HttpMgr::getInstance(), &HttpMgr::sig_login_mod_http_finished,
            this, &LoginDialog::slot_login_mod_http_finished);
    connect(this, &LoginDialog::sig_connect_tcp, &TcpMgr::getInstance(), &TcpMgr::slot_tcp_connect);
    connect(&TcpMgr::getInstance(), &TcpMgr::sig_con_success, this, &LoginDialog::slot_tcp_con_finish);
    connect(&TcpMgr::getInstance(), &TcpMgr::sig_login_failed, this, &LoginDialog::slot_login_failed);
    connect(&TcpMgr::getInstance(), &TcpMgr::sig_switch_chatdlg, this, &LoginDialog::slot_chat_login_success);
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

    const QString email = ui->emailLineEdit->text().trimmed();
    const QString password = ui->pswdLineEdit->text().trimmed();
    if (email.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(
            this,
            QString::fromUtf8(u8"\u767b\u5f55\u5931\u8d25"),
            QString::fromUtf8(u8"\u8bf7\u586b\u5199\u6240\u6709\u5fc5\u586b\u9879\u3002"));
        return;
    }

    QJsonObject jsonObj;
    jsonObj["email"] = email;
    jsonObj["passwd"] = password;
    _loginRequestInFlight = true;
    ui->loginBtn->setEnabled(false);
    HttpMgr::getInstance().PostHttpReq(QUrl(gate_url_prefix + "/user_login"),
                                       jsonObj, ReqId::ID_LOGIN_USER, Modules::LOGINMOD);
}

void LoginDialog::slot_login_mod_http_finished(ReqId id, QString res, ErrorCodes err)
{
    if (err != ErrorCodes::SUCCESS) {
        if (id == ReqId::ID_LOGIN_USER) {
            _loginRequestInFlight = false;
            ui->loginBtn->setEnabled(true);
        }
        QMessageBox::warning(
            this,
            QString::fromUtf8(u8"\u9519\u8bef"),
            QString::fromUtf8(u8"\u7f51\u7edc\u8bf7\u6c42\u5931\u8d25\uff0c\u8bf7\u91cd\u8bd5"));
        return;
    }

    const QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    if (jsonDoc.isNull()) {
        if (id == ReqId::ID_LOGIN_USER) {
            _loginRequestInFlight = false;
            ui->loginBtn->setEnabled(true);
        }
        qDebug() << "[LoginDialog.cpp] [slot_login_mod_http_finished] JSON parse failed:" << res;
        return;
    }
    if (!jsonDoc.isObject()) {
        if (id == ReqId::ID_LOGIN_USER) {
            _loginRequestInFlight = false;
            ui->loginBtn->setEnabled(true);
        }
        qDebug() << "[LoginDialog.cpp] [slot_login_mod_http_finished] JSON is not object:" << res;
        return;
    }
    if (_handlers.contains(id)) {
        _handlers[id](jsonDoc.object());
    }
}

void LoginDialog::initHttpHandlers()
{
    _handlers[ReqId::ID_LOGIN_USER] = [this](const QJsonObject &jsonObj) {
        const int error = jsonObj.value("error").toInt();
        if (error != ErrorCodes::SUCCESS) {
            _loginRequestInFlight = false;
            ui->loginBtn->setEnabled(true);
            qDebug() << "[LoginDialog.cpp] [initHttpHandlers] login failed:" << jsonObj;
            QString errorMessage = jsonObj.value("message").toString();
            if (errorMessage.isEmpty()) {
                errorMessage = QString::fromUtf8(u8"\u7528\u6237\u540d\u6216\u5bc6\u7801\u9519\u8bef\uff0c\u8bf7\u91cd\u8bd5");
            }
            QMessageBox::warning(this,
                                 QString::fromUtf8(u8"\u767b\u5f55\u5931\u8d25"),
                                 errorMessage);
            return;
        }

        ServerInfo si;
        si.Uid = jsonObj["uid"].toInt();
        si.Host = jsonObj["host"].toString();
        si.Port = jsonObj["port"].toString();
        si.Token = jsonObj["token"].toString();

        _uid = si.Uid;
        _token = si.Token;
        qDebug() << "[LoginDialog.cpp] [initHttpHandlers] login http ok, connect chat server:"
                 << si.Uid << si.Host << si.Port;
        emit sig_connect_tcp(si);
    };
}

void LoginDialog::slot_tcp_con_finish(bool bsuccess)
{
    if (bsuccess) {
        QJsonObject jsonObj;
        jsonObj["uid"] = _uid;
        jsonObj["token"] = _token;

        const QJsonDocument doc(jsonObj);
        const QString jsonString = doc.toJson(QJsonDocument::Indented);
        emit TcpMgr::getInstance().sig_send_data(ReqId::ID_CHAT_LOGIN, jsonString);
        return;
    }

    _loginRequestInFlight = false;
    ui->loginBtn->setEnabled(true);
    QMessageBox::warning(
        this,
        QString::fromUtf8(u8"\u9519\u8bef"),
        QString::fromUtf8(u8"TCP \u957f\u94fe\u63a5\u8bf7\u6c42\u5931\u8d25\uff0c\u8bf7\u91cd\u8bd5"));
}

void LoginDialog::slot_login_failed(int err)
{
    _loginRequestInFlight = false;
    ui->loginBtn->setEnabled(true);
    QMessageBox::warning(
        this,
        QString::fromUtf8(u8"\u767b\u5f55\u5931\u8d25"),
        QString::fromUtf8(u8"\u767b\u5f55\u5931\u8d25\uff0c\u9519\u8bef\u7801: ") + QString::number(err));
}

void LoginDialog::slot_chat_login_success()
{
    _loginRequestInFlight = false;
    ui->loginBtn->setEnabled(true);
    saveRememberedCredentials();
}

void LoginDialog::loadRememberedCredentials()
{
    QSettings settings(settingsFilePath(), QSettings::IniFormat);
    const QString email = settings.value(QStringLiteral("login/email")).toString().trimmed();
    const bool rememberPassword = settings.value(QStringLiteral("login/remember_password"), false).toBool();
    const QString password = settings.value(QStringLiteral("login/password")).toString();

    ui->emailLineEdit->setText(email);
    if (_rememberPasswordCheckBox != nullptr) {
        _rememberPasswordCheckBox->setChecked(rememberPassword);
    }
    if (rememberPassword) {
        ui->pswdLineEdit->setText(password);
    } else {
        ui->pswdLineEdit->clear();
    }
}

void LoginDialog::saveRememberedCredentials()
{
    QSettings settings(settingsFilePath(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("login/email"), ui->emailLineEdit->text().trimmed());
    const bool rememberPassword = (_rememberPasswordCheckBox != nullptr) && _rememberPasswordCheckBox->isChecked();
    settings.setValue(QStringLiteral("login/remember_password"), rememberPassword);
    if (rememberPassword) {
        settings.setValue(QStringLiteral("login/password"), ui->pswdLineEdit->text());
    } else {
        settings.remove(QStringLiteral("login/password"));
    }
    settings.sync();
}

QString LoginDialog::settingsFilePath() const
{
    QString settingsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (settingsDir.isEmpty()) {
        settingsDir = QCoreApplication::applicationDirPath();
    }
    QDir dir(settingsDir);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    return dir.filePath(QStringLiteral("login_settings.ini"));
}
