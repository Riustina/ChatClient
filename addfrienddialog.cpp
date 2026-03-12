#include "addfrienddialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

AddFriendDialog::AddFriendDialog(const QString &name, QWidget *parent)
    : QDialog(parent)
    , _messageLabel(new QLabel(this))
    , _confirmButton(new QPushButton(QStringLiteral("确认"), this))
    , _cancelButton(new QPushButton(QStringLiteral("取消"), this))
{
    setWindowTitle(QStringLiteral("添加好友"));
    setModal(true);
    setFixedSize(320, 140);

    _messageLabel->setText(QStringLiteral("确定要添加 [%1] 为好友吗").arg(name));
    _messageLabel->setWordWrap(true);
    _messageLabel->setAlignment(Qt::AlignCenter);

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    buttonLayout->addWidget(_cancelButton);
    buttonLayout->addWidget(_confirmButton);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(18);
    layout->addWidget(_messageLabel);
    layout->addLayout(buttonLayout);

    setStyleSheet(
        "AddFriendDialog { background:#F4F3F9; }"
        "QLabel { font: 10pt 'Microsoft YaHei UI'; color:#1f2937; }"
        "QPushButton { min-width:72px; min-height:32px; border:none; border-radius:16px; font: 10pt 'Microsoft YaHei UI'; }"
        "QPushButton:pressed { padding-top:1px; }");
    _cancelButton->setStyleSheet("background:#EAE9EF; color:#334155;");
    _confirmButton->setStyleSheet("background:#CBCACF; color:#111827;");

    connect(_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(_confirmButton, &QPushButton::clicked, this, &QDialog::accept);
}
