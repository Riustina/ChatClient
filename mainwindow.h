#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "chatpage.h"
#include "logindialog.h"
#include "qstackedwidget.h"
#include "registerdialog.h"
#include "resetdialog.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool event(QEvent *event) override;

private:
    void updateTaskbarFlashState();
    void stopTaskbarFlash();

    Ui::MainWindow *ui;
    QStackedWidget *_stackedWidget;
    ChatPage *_chatPage;
    LoginDialog *_loginDialog;
    RegisterDialog *_registerDialog;
    ResetDialog *_resetDialog;
    bool _friendRequestFlashActive = false;
    bool _chatMessageFlashActive = false;
};
#endif // MAINWINDOW_H
