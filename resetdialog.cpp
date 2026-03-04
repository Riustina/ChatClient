#include "resetdialog.h"
#include "ui_resetdialog.h"

ResetDialog::ResetDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ResetDialog)
{
    ui->setupUi(this);

    connect(ui->cancelBtn, &QPushButton::clicked, this, [this]() {
        emit switchToLogin();
    });
}

ResetDialog::~ResetDialog()
{
    delete ui;
}
