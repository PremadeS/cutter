#ifndef INFO_STORY_H
#define INFO_STORY_H

/*
 * Shared discovery registry for the "Info" user story integration tests.
 *
 * The Info user story is: *"I load a binary and inspect everything about it —
 * functions, strings, sections, imports, flags, symbols. I filter and sort these
 * lists, click an entry to jump the disassembly to it, and expect every view to
 * stay in sync."*
 *
 * Rather than a bespoke test per widget (a nightmare to maintain), every Info
 * dock is described by ONE registry entry below and then exercised by the SAME
 * generic batteries in tst_InfoStory*.cpp. Enrolling a new Info widget is a
 * matter of adding one line to allInfoDocks().
 *
 * Discovery is intentionally dynamic: whether a dock is addressable, has a
 * quick-filter, or has a selector combo is detected at runtime via
 * dynamic_cast/findChild, so the batteries adapt to whatever a widget exposes.
 * Only the class name and a few behavioural hints are hand-specified.
 */

#include "common/AddressableItemModel.h"
#include "core/Cutter.h"
#include "core/MainWindow.h"
#include "widgets/AbstractFilterView.h"
#include "widgets/AddressableItemList.h"
#include "widgets/ComboQuickFilterView.h"
#include "widgets/CutterDockWidget.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QLineEdit>
#include <QList>
#include <QObject>
#include <QSortFilterProxyModel>
#include <QString>

/**
 * @brief One hand-specified entry per Info dock.
 *
 * expectNonEmpty marks docks that a normal loadFile(...,va=1,loadbin=true)+aaa
 * on the fixture MUST populate. Leave it false for docks that are legitimately
 * empty without a debugger/target so their batteries degrade to structural
 * (non-vacuous-guarded) checks instead of failing.
 */
struct InfoDockSpec {
    QString className;
    bool expectNonEmpty = false;
};

/**
 * @brief The full Info widget cast, as instantiated by MainWindow.
 *
 * This mirrors the docks MainWindow::addWidgets() constructs (the windowDocks,
 * windowDocks2 and infoDocks groups). Debug-only docks (stack/threads/…) are
 * deliberately excluded — they belong to the Debugging story, not Info.
 */
inline QList<InfoDockSpec> allInfoDocks()
{
    return {
        // windowDocks / windowDocks2 (always-on Info widgets)
        { "FunctionsWidget", true },
        { "StringsWidget", true },
        { "TypesWidget", true },
        { "CommentsWidget", false },
        // infoDocks (View → Add Info Widgets)
        { "ClassesWidget", false },
        { "EntrypointWidget", false },
        { "ExportsWidget", true },
        { "FlagsWidget", true },
        { "HeadersWidget", true },
        { "ImportsWidget", true },
        { "RelocsWidget", false },
        { "ResourcesWidget", false },
        { "SectionsWidget", true },
        { "SegmentsWidget", true },
        { "SymbolsWidget", true },
        // Globals ("symbols that are also flags") is empty on many binaries
        // including the /bin/ls fixture, so it is not expectNonEmpty.
        { "GlobalsWidget", false },
        { "VTablesWidget", false },
        { "FlirtWidget", false },
    };
}

/**
 * @brief Runtime-discovered structure of one Info dock.
 *
 * Any pointer may be null when the widget does not expose that concept; the
 * batteries skip the relevant check rather than crash.
 */
struct InfoDockUnderTest {
    CutterDockWidget *dock = nullptr;
    QAbstractItemView *view = nullptr;        // the primary item/table/tree view
    QSortFilterProxyModel *proxy = nullptr;   // view->model(), if it is a proxy
    AddressableItemModelI *addressable = nullptr; // proxy/source address() provider
    bool seekableView = false;                // view is an AddressableItemList (activate→seek)
    AbstractFilterView *filterView = nullptr; // quick-filter (text)
    QLineEdit *filterEdit = nullptr;          // the filter's line edit
    QComboBox *combo = nullptr;               // selector combo (section/flagspace/…)
};

inline CutterDockWidget *findDockByName(MainWindow *main, const QString &className)
{
    const auto docks = main->findChildren<CutterDockWidget *>();
    for (CutterDockWidget *d : docks) {
        if (className == QLatin1String(d->metaObject()->className())) {
            return d;
        }
    }
    return nullptr;
}

/**
 * @brief Discover the testable structure of an Info dock.
 *
 * The primary item view is the first QAbstractItemView child. Its model is the
 * proxy (all Info list widgets back their view with an AddressableFilterProxyModel).
 * We cross-cast the model to AddressableItemModelI to reach address()/name(), and
 * cross-cast the view to AddressableItemList to detect the activate→seek wiring
 * (AddressableItemList.h:36-38,91-99).
 */
inline InfoDockUnderTest discoverInfoDock(MainWindow *main, const QString &className)
{
    InfoDockUnderTest u;
    u.dock = findDockByName(main, className);
    if (!u.dock) {
        return u;
    }

    u.view = u.dock->findChild<QAbstractItemView *>();
    if (u.view && u.view->model()) {
        u.proxy = qobject_cast<QSortFilterProxyModel *>(u.view->model());
        u.addressable = dynamic_cast<AddressableItemModelI *>(u.view->model());
        // RTTI cross-cast: succeeds only for AddressableItemList-backed views,
        // which are the ones connecting activated→seekAndShow.
        u.seekableView = (dynamic_cast<AddressableItemList *>(u.view) != nullptr);
    }

    u.filterView = u.dock->findChild<AbstractFilterView *>();
    if (u.filterView) {
        u.filterEdit = u.filterView->findChild<QLineEdit *>();
    }
    // Widgets with a custom combo (Strings section, Flags flagspace, Globals, Types)
    // expose it as a QComboBox child; ComboQuickFilterView widgets nest it there too.
    u.combo = u.dock->findChild<QComboBox *>();
    return u;
}

#endif // INFO_STORY_H
