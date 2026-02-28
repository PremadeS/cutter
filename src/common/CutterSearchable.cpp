#include "CutterSearchable.h"
#include "SearchBarWidget.h"
#include "shortcuts/ShortcutManager.h"

#include <QObject>
#include <QAbstractScrollArea>
#include <QScrollBar>

void CutterSearchableUtil::setupConnections(QWidget *parent, SearchBarWidget *bar)
{
    CutterSearchable *searchable = dynamic_cast<CutterSearchable *>(parent);
    if (!bar || !parent || !searchable) {
        return;
    }

    bar->hide();

    QObject::connect(bar, &SearchBarWidget::searchChanged, parent,
                     [searchable](const QString &text, int options) {
                         searchable->searchChanged(text, options);
                     });

    QObject::connect(bar, &SearchBarWidget::findNextTriggered, parent,
                     [searchable]() { searchable->findNext(); });

    QObject::connect(bar, &SearchBarWidget::findPrevTriggered, parent,
                     [searchable]() { searchable->findPrev(); });

    QObject::connect(bar, &SearchBarWidget::findLastTriggered, parent,
                     [searchable]() { searchable->findLast(); });

    QObject::connect(bar, &SearchBarWidget::hideTriggered, parent,
                     [searchable]() { searchable->searchBarHidden(); });

    QObject::connect(bar, &SearchBarWidget::showTriggered, parent,
                     [searchable]() { searchable->searchBarShown(); });

    QShortcut *shortcut = Shortcuts()->makeQShortcut("Search.toggle", parent);
    QObject::connect(shortcut, &QShortcut::activated, parent, [=]() {
        if (bar->isVisible()) {
            bar->hideSearchBar();
        } else {
            bar->showSearchBar();

            QWidget *area = searchable->searchableArea();
            int h = searchable->searchHPadding();
            int v = searchable->searchVPadding();

            positionSearchBar(parent, bar, area, h, v);
        }
    });
}

void CutterSearchableUtil::positionSearchBar(QWidget *parent, SearchBarWidget *searchBar,
                                             QWidget *area, int hPadding, int vPadding)
{
    if (!searchBar || !area) {
        return;
    }
    int sbWidth = 0;
    if (auto *scrollArea = qobject_cast<QAbstractScrollArea *>(area)) {
        QScrollBar *sb = scrollArea->verticalScrollBar();
        sbWidth = (sb && sb->isVisible()) ? sb->width() : 0;
    }

    QPoint areaPos = area->mapTo(parent, QPoint(0, 0));
    int x = areaPos.x() + area->width() - sbWidth - searchBar->width() - hPadding;
    int y = areaPos.y() + vPadding;

    searchBar->move(x, y);
}
