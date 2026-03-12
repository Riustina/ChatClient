#ifndef SEARCHPOPUPWIDGET_H
#define SEARCHPOPUPWIDGET_H

#include <QFrame>
#include <QVector>
#include "chattypes.h"

class QScrollArea;
class QVBoxLayout;
class QWidget;

class SearchPopupWidget : public QFrame
{
    Q_OBJECT

public:
    explicit SearchPopupWidget(QWidget *parent = nullptr);

    void setSearchText(const QString &text);
    void setResults(const QVector<ContactItem> &results, int selectedId);
    int popupHeight() const;
    static int maxPopupHeight();

signals:
    void addFriendClicked(const QString &text);
    void contactClicked(int contactId);

private:
    void rebuild();
    QWidget *createAddRow();
    QWidget *createEmptyRow();

    QString _searchText;
    QVector<ContactItem> _results;
    int _selectedId = -1;
    QScrollArea *_scrollArea;
    QWidget *_contentWidget;
    QVBoxLayout *_layout;
};

#endif // SEARCHPOPUPWIDGET_H
