#include "registerdialog.h"
#include "ui_registerdialog.h"
#include <QRegularExpression>
#include <QMessageBox>
#include <QJsonObject>
#include "httpmgr.h"

RegisterDialog::RegisterDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RegisterDialog)
{
    ui->setupUi(this);
    setWindowTitle(QString::fromUtf8(u8"注册"));
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
    ui->label->setText(QString::fromUtf8(u8"创建账号"));
    ui->userLabel->setText(QString::fromUtf8(u8"用户名"));
    ui->emailLabel->setText(QString::fromUtf8(u8"邮箱"));
    ui->codeLabel->setText(QString::fromUtf8(u8"验证码"));
    ui->pswdLabel->setText(QString::fromUtf8(u8"密码"));
    ui->sendCodeButton->setText(QString::fromUtf8(u8"获取验证码"));
    ui->cancelButton->setText(QString::fromUtf8(u8"返回登录"));
    ui->registerButton->setText(QString::fromUtf8(u8"注册"));
    ui->userLineEdit->setPlaceholderText(QString::fromUtf8(u8"请输入用户名"));
    ui->emailLineEdit->setPlaceholderText(QString::fromUtf8(u8"请输入邮箱地址"));
    ui->codeLineEdit->setPlaceholderText(QString::fromUtf8(u8"请输入验证码"));
    ui->pswdLineEdit->setPlaceholderText(QString::fromUtf8(u8"请设置登录密码"));

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
            QMessageBox::warning(this, "错误", "获取验证码失败，请重试");
            return;
        }

        auto email = jsonObj.value("email").toString();
        QMessageBox::information(this, "成功", "验证码已发送到 " + email);
    };

    // 注册用户注册回包的逻辑
    _handlers[ReqId::ID_REG_USER] = [this](const QJsonObject& jsonObj) {
        int error = jsonObj.value("error").toInt();
        if (error != ErrorCodes::SUCCESS) {
            qDebug() << "[RegisterDialog.cpp] 函数 [initHttpHandlers] 注册失败: " << jsonObj;
            QMessageBox::warning(this, "注册失败", "注册失败，请重试");
            return;
        }

        QMessageBox::information(this, "成功", "注册成功，请登录");
        emit switchToLogin(); // 注册成功后切换回登录界面
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
    QJsonObject jsonObj;
    jsonObj["email"] = email;
    HttpMgr::getInstance().PostHttpReq(QUrl(gate_url_prefix + "/get_verifycode"), jsonObj, ReqId::ID_GET_VERIFY_CODE, Modules::REGISTERMOD);
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

void RegisterDialog::on_cancelButton_clicked()
{
    emit switchToLogin();
}


void RegisterDialog::on_registerButton_clicked()
{
    // 获取输入
    QString username = ui->userLineEdit->text().trimmed();
    QString email = ui->emailLineEdit->text().trimmed();
    QString verifyCode = ui->codeLineEdit->text().trimmed();
    QString password = ui->pswdLineEdit->text();

    // 快速非空检查
    if (username.isEmpty() || email.isEmpty() ||
        verifyCode.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "注册失败", "请填写所有必填项！");
        return;
    }

    // 构建JSON
    QJsonObject json_obj;
    json_obj["user"] = username;
    json_obj["email"] = email;
    json_obj["verifycode"] = verifyCode;
    json_obj["passwd"] = password;

    // 发送请求
    HttpMgr::getInstance().PostHttpReq(
        QUrl(gate_url_prefix + "/user_register"),
        json_obj,
        ReqId::ID_REG_USER,
        Modules::REGISTERMOD
        );
}

