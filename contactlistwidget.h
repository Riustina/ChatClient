#ifndef CONTACTLISTWIDGET_H
#define CONTACTLISTWIDGET_H

#include <QScrollArea>
#include <QVector>
#include "chattypes.h"

class ContactCell;
class QResizeEvent;

class ContactListWidget : public QScrollArea
{
    Q_OBJECT

public:
    explicit ContactListWidget(QWidget *parent = nullptr);

    void setContacts(const QVector<ContactItem> &contacts, int currentContactId = -1);
    int currentIndex() const;

signals:
    void contactActivated(int index);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void rebuildPool();
    void updateContentHeight();
    void updateVisibleCells();

    QWidget *_contentWidget;
    QVector<ContactItem> _contacts;
    QVector<ContactCell *> _cellPool;
    int _currentIndex = 0;
};

#endif // CONTACTLISTWIDGET_H
