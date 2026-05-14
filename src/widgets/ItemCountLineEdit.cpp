#include "ItemCountLineEdit.h"

#include "Configuration.h"

#include <QLabel>
#include <QResizeEvent>

namespace {
constexpr int padding = 5;
}

ItemCountLineEdit::ItemCountLineEdit(QWidget *parent)
    : QLineEdit(parent),
      mItemCountLabel(new QLabel(this)),
      isItemCountVisible(Config()->getItemCountVisible()),
      itemCountAutoHideEnabled(Config()->getItemCountAutoHide())
{
    // parent widget must handle the context menu
    this->setContextMenuPolicy(Qt::NoContextMenu);

    mItemCountLabel->setStyleSheet("QLabel { background: transparent; }");

    connect(this, &QLineEdit::textChanged, this, &ItemCountLineEdit::updateLabelPosition);
    connect(Config(), &Configuration::itemCountToggled, this, &ItemCountLineEdit::showItemCount);
    connect(Config(), &Configuration::itemCountAutoHideToggled, this,
            &ItemCountLineEdit::setItemCountAutoHide);

    if (!isItemCountVisible) {
        mItemCountLabel->hide();
    }
    updateLabelPosition();
}

void ItemCountLineEdit::setItemCount(int count)
{
    mItemCountLabel->setText(QString("%1 Items").arg(count));
    updateLabelPosition();
}

bool ItemCountLineEdit::itemCountVisible() const
{
    return isItemCountVisible;
}

void ItemCountLineEdit::setItemCountAutoHide(bool value)
{
    itemCountAutoHideEnabled = value;
    updateLabelPosition();
}

bool ItemCountLineEdit::itemCountAutoHide() const
{
    return itemCountAutoHideEnabled;
}

void ItemCountLineEdit::showItemCount(bool show)
{
    isItemCountVisible = show;
    mItemCountLabel->setVisible(show);
    if (!show) {
        this->setTextMargins(0, 0, 0, 0);
    }
    updateLabelPosition();
}

void ItemCountLineEdit::resizeEvent(QResizeEvent *event)
{
    QLineEdit::resizeEvent(event);
    updateLabelPosition();
}

void ItemCountLineEdit::updateLabelPosition()
{
    if (!isItemCountVisible) {
        return;
    }

    mItemCountLabel->adjustSize();

    const int labelWidth = mItemCountLabel->sizeHint().width();
    const int x = this->width() - labelWidth - padding;

    if (itemCountAutoHideEnabled) {
        const QFont font = this->font();
        const QFontMetrics fm(font);
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
        int const textWidth = fm.horizontalAdvance(this->text());
#else
        int textWidth = fm.width(this->text());
#endif
        const int buffer = Config()->windowColorIsDark() ? 4 : 5;
        if (x <= (padding * buffer) + textWidth) {
            mItemCountLabel->hide();
            this->setTextMargins(0, 0, 0, 0);
            return;
        }
    }

    // required so the label doesn't overlap with placeholder text
    auto margins = this->textMargins();
    margins.setRight(labelWidth + padding);
    this->setTextMargins(margins);

    const int y = (this->height() - mItemCountLabel->height()) / 2;
    mItemCountLabel->move(x, y);
    mItemCountLabel->show();
}
