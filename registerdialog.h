#ifndef REGISTERDIALOG_H
#define REGISTERDIALOG_H

#include <QDialog>
#include "global.h"

namespace Ui {
class RegisterDialog;
}

class RegisterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegisterDialog(QWidget *parent = nullptr);
    ~RegisterDialog();

private slots:
    void on_sendCodeButton_clicked();
    void slot_reg_mod_http_finished(ReqId id, QString res, ErrorCodes err);

    void on_cancelButton_clicked();

    void on_registerButton_clicked();

private:
    Ui::RegisterDialog *ui;
    void initHttpHandlers();
    QMap<ReqId, std::function<void(const QJsonObject&)>> _handlers; // 请求ID到处理函数的映射

signals:
    void switchToLogin();
};

#endif // REGISTERDIALOG_H
