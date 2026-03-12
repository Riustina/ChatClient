#ifndef ADDFRIENDDIALOG_H
#define ADDFRIENDDIALOG_H

#include <QDialog>

class QLabel;
class QPushButton;

class AddFriendDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddFriendDialog(const QString &name, QWidget *parent = nullptr);

private:
    QLabel *_messageLabel;
    QPushButton *_confirmButton;
    QPushButton *_cancelButton;
};

#endif // ADDFRIENDDIALOG_H
