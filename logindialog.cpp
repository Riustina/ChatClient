#include "logindialog.h"
#include "ui_logindialog.h"
#include <QMessageBox>
#include <QJsonObject>
#include "httpmgr.h"
#include "TcpMgr.h"

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
{
    ui->setupUi(this);

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
    //连接tcp连接请求的信号和槽函数
    connect(this, &LoginDialog::sig_connect_tcp, &TcpMgr::getInstance(), &TcpMgr::slot_tcp_connect);
    //连接tcp管理者发出的连接成功信号
    connect(&TcpMgr::getInstance(), &TcpMgr::sig_con_success, this, &LoginDialog::slot_tcp_con_finish);
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

void LoginDialog::on_loginBtn_clicked()
{
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
    HttpMgr::getInstance().PostHttpReq(QUrl(gate_url_prefix+"/user_login"),
                                        json_obj, ReqId::ID_LOGIN_USER,Modules::LOGINMOD);
}

void LoginDialog::slot_login_mod_http_finished(ReqId id, QString res, ErrorCodes err)
{
    if (err != ErrorCodes::SUCCESS) {
        QMessageBox::warning(this, "错误", "网络请求失败，请重试");
        return;
    }

    // 解析 JSON 字符串，转化为QByteArray
    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    if (jsonDoc.isNull()) {
        qDebug() << "[LoginDialog.cpp] 函数 [slot_login_mod_http_finished] JSON 解析失败: " << res;
        return;
    }
    if (!jsonDoc.isObject()) {
        qDebug() << "[LoginDialog.cpp] 函数 [slot_login_mod_http_finished] JSON 不是对象: " << res;
        return;
    }
    _handlers[id](jsonDoc.object());
}

void LoginDialog::initHttpHandlers() {
    _handlers[ReqId::ID_LOGIN_USER] = [this](const QJsonObject& jsonObj) {
        int error = jsonObj.value("error").toInt();
        if (error != ErrorCodes::SUCCESS) {
            qDebug() << "[LoginDialog.cpp] 函数 [initHttpHandlers] 登录失败: " << jsonObj;
            QMessageBox::warning(this, "登录失败", "用户名或密码错误，请重试");
            return;
        }
        auto user = jsonObj["user"].toString();

        //发送信号通知tcpMgr发送长链接
        ServerInfo si;
        si.Uid = jsonObj["uid"].toInt();
        si.Host = jsonObj["host"].toString();
        si.Port = jsonObj["port"].toString();
        si.Token = jsonObj["token"].toString();

        _uid = si.Uid;
        _token = si.Token;
        qDebug()<< "user is " << user << " uid is " << si.Uid <<" host is "
                 << si.Host << " Port is " << si.Port << " Token is " << si.Token;
        emit sig_connect_tcp(si);
        QMessageBox::information(this, "成功", "登录成功");
    };
}


void LoginDialog::slot_tcp_con_finish(bool bsuccess)
{

    if(bsuccess){
        QJsonObject jsonObj;
        jsonObj["uid"] = _uid;
        jsonObj["token"] = _token;

        QJsonDocument doc(jsonObj);
        QString jsonString = doc.toJson(QJsonDocument::Indented);

        //发送tcp请求给chat server
        TcpMgr::getInstance().sig_send_data(ReqId::ID_CHAT_LOGIN, jsonString);

    }else{
        QMessageBox::warning(this, "错误", "TCP长链接请求失败，请重试");
    }

}
