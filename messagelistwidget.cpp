#include "messagelistwidget.h"

#include "messagecell.h"

#include <QScrollBar>

MessageListWidget::MessageListWidget(QWidget *parent)
    : QScrollArea(parent)
    , _contentWidget(new QWidget(this))
{
    setAttribute(Qt::WA_StyledBackground, true);
    setWidget(_contentWidget);
    setWidgetResizable(false);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setAlignment(Qt::AlignTop | Qt::AlignLeft);
    viewport()->setAttribute(Qt::WA_StyledBackground, true);
    viewport()->setStyleSheet("background:#F4F3F9;");
    _contentWidget->setAttribute(Qt::WA_StyledBackground, true);
    _contentWidget->setStyleSheet("background:#F4F3F9;");
    setStyleSheet("QScrollArea { background:#F4F3F9; border:none; }");

    _refreshDebounceTimer = new QTimer(this);
    _refreshDebounceTimer->setSingleShot(true);
    _refreshDebounceTimer->setInterval(0);

    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int newValue) {
        _autoFollowLatest = isNearBottom();
        if (verticalScrollBar()->maximum() > 0 && isNearTop()) {
            if (_topSignalArmed) {
                _topSignalArmed = false;
                QMetaObject::invokeMethod(this, [this]() {
                    emit reachedTop();
                }, Qt::QueuedConnection);
            }
        } else {
            _topSignalArmed = true;
        }
        updateVisibleCells();
    });

    connect(_refreshDebounceTimer, &QTimer::timeout, this, [this]() {
        if (!_pendingRefreshMessages.isEmpty()) {
            refreshMessagesPreservePositionImmediate(_pendingRefreshMessages);
            _pendingRefreshMessages.clear();
        }
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

void MessageListWidget::refreshMessagesPreservePosition(const QVector<MessageItem> &messages)
{
    _pendingRefreshMessages = messages;  // 后来的覆盖前面的，只保留最新一份
    _refreshDebounceTimer->start();      // 已经在跑就重置，同一事件循环里多次调用只触发一次
}

void MessageListWidget::refreshMessagesPreservePositionImmediate(const QVector<MessageItem> &messages)
{
    const bool wasNearBottom = isNearBottom();

    if (wasNearBottom) {
        _messages = messages;
        rebuildPool();
        recalculateLayout();
        updateVisibleCells();
        scrollToBottom();
        return;
    }

    const int oldMax = verticalScrollBar()->maximum();
    const int oldBottomOffset = oldMax - verticalScrollBar()->value();

    // 同样：先屏蔽，再 resize
    viewport()->setUpdatesEnabled(false);
    verticalScrollBar()->blockSignals(true);

    _messages = messages;
    rebuildPool();
    recalculateLayout();

    verticalScrollBar()->setValue(qMax(0, verticalScrollBar()->maximum() - oldBottomOffset));

    verticalScrollBar()->blockSignals(false);
    updateVisibleCells();
    viewport()->setUpdatesEnabled(true);
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

void MessageListWidget::prependMessages(const QVector<MessageItem> &messages)
{
    if (messages.isEmpty()) return;

    const int availableWidth = viewport()->width();
    int addedHeight = 0;
    for (const auto &msg : messages) {
        addedHeight += MessageCell::heightForMessage(msg, availableWidth) + 4;
    }

    const int oldValue = verticalScrollBar()->value();
    _messages = messages + _messages;

    const int newContentHeight = [&]() {
        int y = 4;
        for (const auto &msg : _messages)
            y += MessageCell::heightForMessage(msg, availableWidth) + 4;
        return y + 4 + 1;
    }();
    const int newMax = qMax(0, newContentHeight - viewport()->height());
    const int newValue = qMin(oldValue + addedHeight, newMax);

    verticalScrollBar()->blockSignals(true);
    verticalScrollBar()->setRange(0, newMax);
    verticalScrollBar()->setValue(newValue);
    verticalScrollBar()->blockSignals(false);

    // rebuildPool();
    recalculateLayout();
    updateVisibleCells();

    // 修复：内容撑不满 viewport 时，maximum=0，重置 autoFollowLatest 状态
    // 避免后续 scrollToBottom 被意外触发
    if (newMax == 0) {
        _autoFollowLatest = false;
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
        connect(cell, &MessageCell::retryRequested, this, &MessageListWidget::retryRequested);
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

    _contentWidget->resize(availableWidth, currentY + 4 + 1);
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

bool MessageListWidget::isNearTop() const
{
    return verticalScrollBar()->value() <= 12;
}

void MessageListWidget::scrollToBottom()
{
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    _autoFollowLatest = true;
}
