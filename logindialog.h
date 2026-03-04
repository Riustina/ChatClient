#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include "global.h"
#include <QDialog>

namespace Ui {
class LoginDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

private:
    Ui::LoginDialog *ui;
    QMap<ReqId, std::function<void(const QJsonObject&)>> _handlers;

    void initHttpHandlers();

signals:
    void switchRegister(); // 切换到注册界面的信号
    void switchReset();    // 切换到重置界面的信号
private slots:
    void on_loginBtn_clicked();
    void slot_login_mod_http_finished(ReqId id, QString res, ErrorCodes err);
};

#endif // LOGINDIALOG_H
