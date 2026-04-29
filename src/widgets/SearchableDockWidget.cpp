#include "SearchableDockWidget.h"
#include "SearchBarWidget.h"

#include <QScrollBar>
#include <QAbstractScrollArea>
#include <QTimer>

namespace {
constexpr int hPadding = 7;
constexpr int vPadding = 4;
};

SearchableDockWidget::SearchableDockWidget(MainWindow *parent)
    : CutterDockWidget(parent), mSearchBar(new SearchBarWidget(this))
{
    CutterSearchableHelper::setupConnections(this, mSearchBar);
}

void SearchableDockWidget::resizeEvent(QResizeEvent *event)
{
    CutterDockWidget::resizeEvent(event);
    updateSearchBarPosition();
}

void SearchableDockWidget::updateSearchBarPosition()
{
    CutterSearchableHelper::positionSearchBar(this, mSearchBar, searchableArea(), searchHPadding(),
                                              searchVPadding());
}

int SearchableDockWidget::searchHPadding() const
{
    return hPadding;
}

int SearchableDockWidget::searchVPadding() const
{
    return vPadding;
}
