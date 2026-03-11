#include "messagelistwidget.h"

#include "messagecell.h"

#include <QScrollBar>

MessageListWidget::MessageListWidget(QWidget *parent)
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
        _autoFollowLatest = isNearBottom();
        updateVisibleCells();
    });
}

void MessageListWidget::setMessages(const QVector<MessageItem> &messages)
{
    _messages = messages;
    rebuildPool();
    recalculateLayout();
    updateVisibleCells();
    scrollToBottom();
}

void MessageListWidget::appendMessage(const MessageItem &message)
{
    const bool shouldFollow = _autoFollowLatest || isNearBottom();
    _messages.push_back(message);
    recalculateLayout();
    updateVisibleCells();
    if (shouldFollow) {
        scrollToBottom();
    }
}

void MessageListWidget::resizeEvent(QResizeEvent *event)
{
    QScrollArea::resizeEvent(event);
    rebuildPool();
    recalculateLayout();
    updateVisibleCells();
}

void MessageListWidget::rebuildPool()
{
    const int visibleCount = qMax(1, viewport()->height() / 92 + 4);
    while (_cellPool.size() < visibleCount) {
        auto *cell = new MessageCell(_contentWidget);
        cell->hide();
        _cellPool.push_back(cell);
    }
}

void MessageListWidget::recalculateLayout()
{
    _offsets.resize(_messages.size());
    _heights.resize(_messages.size());

    int currentY = 4;
    const int availableWidth = viewport()->width();
    for (int i = 0; i < _messages.size(); ++i) {
        _offsets[i] = currentY;
        _heights[i] = MessageCell::heightForMessage(_messages[i], availableWidth);
        currentY += _heights[i] + 4;
    }

    _contentWidget->resize(availableWidth, qMax(currentY + 4, viewport()->height()));
}

void MessageListWidget::updateVisibleCells()
{
    if (_cellPool.isEmpty()) {
        return;
    }

    const int scrollTop = verticalScrollBar()->value();
    const int scrollBottom = scrollTop + viewport()->height();

    int firstIndex = 0;
    while (firstIndex < _offsets.size() && _offsets[firstIndex] + _heights[firstIndex] < scrollTop) {
        ++firstIndex;
    }

    int poolIndex = 0;
    for (int modelIndex = firstIndex; modelIndex < _messages.size() && poolIndex < _cellPool.size(); ++modelIndex) {
        if (_offsets[modelIndex] > scrollBottom + 80) {
            break;
        }

        auto *cell = _cellPool[poolIndex++];
        cell->setGeometry(0, _offsets[modelIndex], viewport()->width(), _heights[modelIndex]);
        cell->setMessage(_messages[modelIndex], viewport()->width());
        cell->show();
    }

    for (int i = poolIndex; i < _cellPool.size(); ++i) {
        _cellPool[i]->hide();
    }
}

bool MessageListWidget::isNearBottom() const
{
    return verticalScrollBar()->value() >= verticalScrollBar()->maximum() - 24;
}

void MessageListWidget::scrollToBottom()
{
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    _autoFollowLatest = true;
}
