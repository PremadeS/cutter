#ifndef ADDRESS_RANGE_SCROLLBAR_H
#define ADDRESS_RANGE_SCROLLBAR_H

#include <QScrollBar>
#include "CutterCommon.h"

class AddressRangeScrollBar : public QScrollBar
{
    Q_OBJECT
public:
    AddressRangeScrollBar(QWidget *parent = nullptr);
    void refreshRange();
    void setPosition(RVA address);
    RVA address();

    RVA clampAddressToRange(RVA address);
    RVA rangeSize();

    // This is requried for the scrollbar to appear on macOS when show scroll bars option is
    // set to "When Scrolling"
    void triggerNativeWheel(QWheelEvent *event);

signals:
    void hideScrollBar();
    void showScrollBar();
    void scrolled(int lines);

protected:
    void wheelEvent(QWheelEvent *event) override;

private:
    RVA beginOffset = 0, endOffset = RVA_INVALID;
    int accumScrollWheelDeltaY = 0;
};

#endif // ADDRESS_RANGE_SCROLLBAR_H
