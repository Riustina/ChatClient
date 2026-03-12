#include "searchpopupwidget.h"

#include "contactcell.h"

#include <QLabel>
#include <QMouseEvent>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {
constexpr int kAddRowHeight = 42;
constexpr int kEmptyRowHeight = 42;

class ClickableRow : public QFrame
{
    Q_OBJECT
public:
    explicit ClickableRow(QWidget *parent = nullptr)
        : QFrame(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_StyledBackground, true);
    }

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        QFrame::mousePressEvent(event);
        if (event->button() == Qt::LeftButton) {
            emit clicked();
        }
    }
};
}

SearchPopupWidget::SearchPopupWidget(QWidget *parent)
    : QFrame(parent)
    , _scrollArea(new QScrollArea(this))
    , _contentWidget(new QWidget(this))
    , _layout(new QVBoxLayout(_contentWidget))
{
    setAttribute(Qt::WA_StyledBackground, true);
    setFocusPolicy(Qt::NoFocus);
    setObjectName("searchPopupWidget");
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(0);

    _scrollArea->setFrameShape(QFrame::NoFrame);
    _scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    _scrollArea->setWidgetResizable(true);
    _scrollArea->setFocusPolicy(Qt::NoFocus);
    _scrollArea->setWidget(_contentWidget);
    rootLayout->addWidget(_scrollArea);

    _layout->setContentsMargins(8, 8, 8, 8);
    _layout->setSpacing(4);

    setStyleSheet(
        "QFrame#searchPopupWidget { background:#F4F3F9; border:1px solid #dfdde7; border-radius:14px; }"
        "QFrame#searchAddRow { background:transparent; border-radius:10px; }"
        "QFrame#searchAddRow:hover { background:#EAE9EF; }"
        "QLabel#searchAddLabel { font: 10pt 'Microsoft YaHei UI'; color:#1f2937; padding:10px 12px; }"
        "QFrame#searchEmptyRow { background:transparent; }"
        "QLabel#searchEmptyLabel { font: 10pt 'Microsoft YaHei UI'; color:#64748b; padding:10px 12px; }"
        "QScrollArea { background:transparent; border:none; }"
        "QWidget { background:transparent; }");
}

void SearchPopupWidget::setSearchText(const QString &text)
{
    _searchText = text.trimmed();
    rebuild();
}

void SearchPopupWidget::setResults(const QVector<ContactItem> &results, int selectedId)
{
    _results = results;
    _selectedId = selectedId;
    rebuild();
}

void SearchPopupWidget::rebuild()
{
    while (QLayoutItem *item = _layout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            delete widget;
        }
        delete item;
    }

    _layout->addWidget(createAddRow());

    if (_results.isEmpty()) {
        _layout->addWidget(createEmptyRow());
    } else {
        for (const ContactItem &contact : _results) {
            auto *cell = new ContactCell(_contentWidget);
            cell->setContact(contact);
            cell->setSelected(contact.id == _selectedId);
            connect(cell, &ContactCell::clicked, this, [this, contact]() {
                emit contactClicked(contact.id);
            });
            _layout->addWidget(cell);
        }
    }

    _layout->addStretch();
    _layout->activate();
    _contentWidget->adjustSize();

    const int viewportHeight = qMin(popupHeight(), maxPopupHeight());
    _scrollArea->setFixedHeight(viewportHeight);
    setFixedHeight(viewportHeight + 16);
}

int SearchPopupWidget::popupHeight() const
{
    const int resultCount = _results.isEmpty() ? 1 : _results.size();
    const int margins = _layout->contentsMargins().top() + _layout->contentsMargins().bottom();
    const int spacing = _layout->spacing() * resultCount;
    const int bodyHeight = _results.isEmpty() ? kEmptyRowHeight : _results.size() * ContactCell::cellHeight();
    return margins + kAddRowHeight + bodyHeight + spacing;
}

int SearchPopupWidget::maxPopupHeight()
{
    return 320;
}

QWidget *SearchPopupWidget::createAddRow()
{
    auto *row = new ClickableRow(_contentWidget);
    row->setObjectName("searchAddRow");
    row->setFixedHeight(kAddRowHeight);

    auto *layout = new QVBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *label = new QLabel(row);
    label->setObjectName("searchAddLabel");
    label->setText(_searchText.isEmpty()
                   ? QStringLiteral("添加好友")
                   : QStringLiteral("添加好友 [%1]").arg(_searchText));
    layout->addWidget(label);

    connect(row, &ClickableRow::clicked, this, [this]() {
        emit addFriendClicked(_searchText);
    });

    return row;
}

QWidget *SearchPopupWidget::createEmptyRow()
{
    auto *row = new QFrame(_contentWidget);
    row->setObjectName("searchEmptyRow");
    row->setFixedHeight(kEmptyRowHeight);

    auto *layout = new QVBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *label = new QLabel(QStringLiteral("无匹配结果"), row);
    label->setObjectName("searchEmptyLabel");
    layout->addWidget(label);

    return row;
}

#include "searchpopupwidget.moc"
