#ifndef RESETDIALOG_H
#define RESETDIALOG_H

#include <QDialog>
#include <QMap>
#include <QJsonObject>
#include <functional>
#include "global.h"

namespace Ui {
class ResetDialog;
}

class ResetDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ResetDialog(QWidget *parent = nullptr);
    ~ResetDialog();

private slots:
    void on_getCodeBtn_clicked();
    void on_cancelBtn_clicked();
    void on_resetBtn_clicked();
    void slot_reset_mod_http_finished(ReqId id, QString res, ErrorCodes err);

private:
    Ui::ResetDialog *ui;
    void initHttpHandlers();
    QMap<ReqId, std::function<void(const QJsonObject&)>> _handlers;

signals:
    void switchToLogin();
};

#endif // RESETDIALOG_H
