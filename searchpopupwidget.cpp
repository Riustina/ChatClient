#include "searchpopupwidget.h"

#include "contactcell.h"

#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int kAddRowHeight = 42;
constexpr int kEmptyRowHeight = 42;
constexpr int kOuterPadding = 8;
constexpr int kInnerPadding = 8;
constexpr int kRowSpacing = 4;

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
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        QFrame::mouseReleaseEvent(event);
        if (event->button() == Qt::LeftButton) {
            QTimer::singleShot(0, this, [this]() {
                emit clicked();
            });
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

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(26);
    shadow->setOffset(0, 10);
    shadow->setColor(QColor(130, 120, 150, 55));
    setGraphicsEffect(shadow);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(kOuterPadding, kOuterPadding, kOuterPadding, kOuterPadding);
    rootLayout->setSpacing(0);

    _scrollArea->setFrameShape(QFrame::NoFrame);
    _scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    _scrollArea->setWidgetResizable(false);
    _scrollArea->setFocusPolicy(Qt::NoFocus);
    _scrollArea->setWidget(_contentWidget);
    rootLayout->addWidget(_scrollArea);

    _layout->setContentsMargins(kInnerPadding, kInnerPadding, kInnerPadding, kInnerPadding);
    _layout->setSpacing(kRowSpacing);

    setStyleSheet(
        "QFrame#searchPopupWidget { background:#FCF8FF; border:1px solid #e7e0ef; border-radius:14px; }"
        "QFrame#searchAddRow { background:transparent; border-radius:10px; }"
        "QFrame#searchAddRow:hover { background:#EAE9EF; }"
        "QLabel#searchAddLabel { font: 10pt 'Microsoft YaHei UI'; color:#1f2937; padding:10px 12px; }"
        "QFrame#searchEmptyRow { background:transparent; }"
        "QLabel#searchEmptyLabel { font: 10pt 'Microsoft YaHei UI'; color:#64748b; padding:10px 12px; }"
        "QScrollArea { background:transparent; border:none; }"
        "QScrollBar:vertical { background:transparent; width:8px; margin:6px 2px 6px 0px; }"
        "QScrollBar::handle:vertical { background:#d9d2e2; border-radius:4px; min-height:30px; }"
        "QScrollBar::handle:vertical:hover { background:#c8bfd3; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background:transparent; }"
        "QWidget { background:transparent; }");
}

void SearchPopupWidget::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    _contentWidget->setFixedWidth(_scrollArea->viewport()->width());
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

    if (hasActiveQuery()) {
        _layout->addWidget(createAddRow());
    }

    if (!hasActiveQuery() || _results.isEmpty()) {
        _layout->addWidget(createEmptyRow());
    } else {
        for (const ContactItem &contact : _results) {
            auto *cell = new ContactCell(_contentWidget);
            cell->setContact(contact);
            cell->setSelected(contact.id == _selectedId);
            connect(cell, &ContactCell::clicked, this, [this, contact]() {
                QTimer::singleShot(0, this, [this, contact]() {
                    emit contactClicked(contact.id);
                });
            });
            _layout->addWidget(cell);
        }
    }

    _layout->activate();

    const int innerHeight = contentHeight();
    const int viewportHeight = qMin(innerHeight, maxPopupHeight());
    _contentWidget->setFixedSize(_scrollArea->viewport()->width(), innerHeight);
    _scrollArea->setFixedHeight(viewportHeight);
    setFixedHeight(viewportHeight + (kOuterPadding * 2));
}

int SearchPopupWidget::popupHeight() const
{
    return contentHeight() + (kOuterPadding * 2);
}

int SearchPopupWidget::maxPopupHeight()
{
    return 320;
}

int SearchPopupWidget::contentHeight() const
{
    const int margins = _layout->contentsMargins().top() + _layout->contentsMargins().bottom();
    int rowCount = 1;
    int totalHeight = kEmptyRowHeight;

    if (hasActiveQuery()) {
        rowCount = 1;
        totalHeight = kAddRowHeight;
        if (_results.isEmpty()) {
            ++rowCount;
            totalHeight += kEmptyRowHeight;
        } else {
            rowCount += _results.size();
            totalHeight += _results.size() * ContactCell::cellHeight();
        }
    }

    const int spacing = qMax(0, rowCount - 1) * _layout->spacing();
    return margins + totalHeight + spacing;
}

bool SearchPopupWidget::hasActiveQuery() const
{
    return !_searchText.isEmpty();
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
    label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    layout->addWidget(label);

    return row;
}

#include "searchpopupwidget.moc"
