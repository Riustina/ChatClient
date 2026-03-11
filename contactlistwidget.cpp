#include "contactlistwidget.h"

#include "contactcell.h"

#include <QScrollBar>

ContactListWidget::ContactListWidget(QWidget *parent)
    : QScrollArea(parent)
    , _contentWidget(new QWidget(this))
{
    setWidget(_contentWidget);
    setWidgetResizable(false);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setAlignment(Qt::AlignTop | Qt::AlignLeft);
    _contentWidget->setAttribute(Qt::WA_StyledBackground, true);
    _contentWidget->setStyleSheet("background:transparent;");

    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        updateVisibleCells();
    });
}

void ContactListWidget::setContacts(const QVector<ContactItem> &contacts)
{
    _contacts = contacts;
    if (_currentIndex >= _contacts.size()) {
        _currentIndex = 0;
    }
    rebuildPool();
    updateContentHeight();
    updateVisibleCells();
}

int ContactListWidget::currentIndex() const
{
    return _currentIndex;
}

void ContactListWidget::resizeEvent(QResizeEvent *event)
{
    QScrollArea::resizeEvent(event);
    rebuildPool();
    updateContentHeight();
    updateVisibleCells();
}

void ContactListWidget::rebuildPool()
{
    const int visibleCount = qMax(1, viewport()->height() / ContactCell::cellHeight() + 3);
    while (_cellPool.size() < visibleCount) {
        auto *cell = new ContactCell(_contentWidget);
        cell->hide();
        connect(cell, &ContactCell::clicked, this, [this, cell]() {
            const int index = cell->property("modelIndex").toInt();
            if (index < 0 || index >= _contacts.size()) {
                return;
            }
            _currentIndex = index;
            updateVisibleCells();
            emit contactActivated(index);
        });
        _cellPool.push_back(cell);
    }
}

void ContactListWidget::updateContentHeight()
{
    _contentWidget->resize(viewport()->width(), qMax(_contacts.size() * ContactCell::cellHeight(), viewport()->height()));
}

void ContactListWidget::updateVisibleCells()
{
    const int firstIndex = qMax(0, verticalScrollBar()->value() / ContactCell::cellHeight());

    for (int i = 0; i < _cellPool.size(); ++i) {
        auto *cell = _cellPool[i];
        const int modelIndex = firstIndex + i;
        if (modelIndex >= _contacts.size()) {
            cell->hide();
            continue;
        }

        cell->setGeometry(0, modelIndex * ContactCell::cellHeight(), viewport()->width(), ContactCell::cellHeight());
        cell->setProperty("modelIndex", modelIndex);
        cell->setContact(_contacts[modelIndex]);
        cell->setSelected(modelIndex == _currentIndex);
        cell->show();
    }
}
