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

private:
    Ui::RegisterDialog *ui;
};

#endif // REGISTERDIALOG_H
