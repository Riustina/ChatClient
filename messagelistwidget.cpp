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

    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) {
        _autoFollowLatest = isNearBottom();
        checkIfReachedTop(); // 调用统一检测
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

    // 3. 重新计算布局
    const int availableWidth = viewport()->width();
    const int newHeight = MessageCell::heightForMessage(message, availableWidth);
    const int newOffset = _messages.size() >= 2
                              ? _offsets.back() + _heights.back() + 4
                              : 4;
    _offsets.push_back(newOffset);
    _heights.push_back(newHeight);

    const int newContentHeight = newOffset + newHeight + 4 + 1;
    _contentWidget->resize(availableWidth, newContentHeight);

    rebuildPool();
    updateVisibleCells();

    // 4. 处理滚动和新消息提示
    if (shouldFollow) {
        // 自动滚动到底部
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

    recalculateLayout();

    const int newMax = qMax(0, _contentWidget->height() - viewport()->height());
    const int newValue = qMin(oldValue + addedHeight, newMax);

    // blockSignals 会阻止 Qt 移动 _contentWidget，导致 cell 渲染到屏幕外不可见。
    // 改用 setUpdatesEnabled(false) 抑制中间帧闪烁，同时保留 Qt 内部的 geometry 更新。
    viewport()->setUpdatesEnabled(false);
    verticalScrollBar()->setRange(0, newMax);
    verticalScrollBar()->setValue(newValue);
    viewport()->setUpdatesEnabled(true);

    rebuildPool();
    updateVisibleCells();

    if (newMax == 0) {
        _autoFollowLatest = false;
    }
}

void MessageListWidget::notifyMessageHeightChanged(int modelIndex, const MessageItem &updated)
{
    if (modelIndex < 0 || modelIndex >= _messages.size()) {
        return;
    }

    // 先同步内部拷贝，否则 cell 渲染时拿到的 image 仍然是空的
    _messages[modelIndex] = updated;

    const int availableWidth = viewport()->width();
    const int oldHeight = _heights[modelIndex];
    const int newHeight = MessageCell::heightForMessage(_messages[modelIndex], availableWidth);

    if (oldHeight == newHeight) {
        updateVisibleCells();
        return;
    }

    const int delta = newHeight - oldHeight;
    _heights[modelIndex] = newHeight;

    for (int i = modelIndex + 1; i < _offsets.size(); ++i) {
        _offsets[i] += delta;
    }

    const int lastIndex = _messages.size() - 1;
    const int newContentHeight = _offsets[lastIndex] + _heights[lastIndex] + 4 + 1;
    _contentWidget->resize(availableWidth, newContentHeight);

    const int scrollTop = verticalScrollBar()->value();
    const int rowBottom = _offsets[modelIndex] + oldHeight;

    const int newMax = qMax(0, newContentHeight - viewport()->height());

    if (rowBottom <= scrollTop) {
        const int compensatedValue = qMin(scrollTop + delta, newMax);
        verticalScrollBar()->blockSignals(true);
        verticalScrollBar()->setRange(0, newMax);
        verticalScrollBar()->setValue(compensatedValue);
        verticalScrollBar()->blockSignals(false);
        updateVisibleCells();
    } else {
        const int currentValue = qMin(verticalScrollBar()->value(), newMax);
        verticalScrollBar()->blockSignals(true);
        verticalScrollBar()->setRange(0, newMax);
        verticalScrollBar()->setValue(currentValue);
        verticalScrollBar()->blockSignals(false);

        if (_autoFollowLatest || isNearBottom()) {
            scrollToBottom();
        } else {
            updateVisibleCells();
        }
    }
}

void MessageListWidget::resizeEvent(QResizeEvent *event) {
    QScrollArea::resizeEvent(event);
    recalculateLayout();
    updateVisibleCells();
    checkIfReachedTop();
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

    const int scrollTop    = verticalScrollBar()->value();
    const int scrollBottom = scrollTop + viewport()->height();

    int firstIndex = 0;
    {
        int lo = 0, hi = (int)_offsets.size() - 1;
        while (lo <= hi) {
            int mid = (lo + hi) / 2;
            if (_offsets[mid] + _heights[mid] < scrollTop) {
                lo = mid + 1;
            } else {
                hi = mid - 1;
            }
        }
        firstIndex = lo;
    }

    int poolIndex = 0;
    for (int modelIndex = firstIndex;
         modelIndex < _messages.size() && poolIndex < _cellPool.size();
         ++modelIndex)
    {
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

void MessageListWidget::checkIfReachedTop() {
    // 逻辑优化：
    // 如果 maximum 为 0，说明内容还没填满窗口，这本身就应该触发一次“加载更多”来尝试填满
    bool isAtTop = (verticalScrollBar()->value() <= 50);

    if (isAtTop) {
        if (_topSignalArmed) {
            _topSignalArmed = false; // 触发后立即锁定，防止重复触发
            QMetaObject::invokeMethod(this, [this]() {
                emit reachedTop();
            }, Qt::QueuedConnection);
        }
    } else {
        // 只有离开顶部区域后，才重新允许触发（重装弹药）
        _topSignalArmed = true;
    }
}

void MessageListWidget::scrollToBottom()
{
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    _autoFollowLatest = true;
}
