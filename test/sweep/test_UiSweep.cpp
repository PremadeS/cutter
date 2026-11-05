/**
 * @file test_UiSweep.cpp
 * @brief The main window's menus, toolbars, docks and context menus work without crashing.
 *
 * The test acts as a user and nothing is listed by hand. It clicks every menu-bar entry
 * (opening submenus by hovering), every toolbar button and toolbar dropdown entry, and presses
 * every shortcut that isn't in a menu. It opens every dock, and every dock's context menus by
 * right-clicking each of its views, without clicking their entries. All of it is found at run
 * time, so new widgets, menus and entries are covered without editing this file. Every dialog
 * and message box that opens is rejected as soon as it is shown.
 *
 * Every dock opens again under ESIL emulation. Entries that would leave the test or touch the
 * fixture are in deniedActions.
 */

#include "AsyncTask.h"
#include "CutterDockWidget.h"
#include "DeadlockWatchdog.h"
#include "DebugActions.h"
#include "Fixtures.h"
#include "MemoryDockWidget.h"
#include "TestEnv.h"
#include "core/MainWindow.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDebug>
#include <QDialog>
#include <QHeaderView>
#include <QHash>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>
#include <QSet>
#include <QTest>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <utility>

namespace {

constexpr int settleTimeoutMs = 60000;
constexpr int menuTimeoutMs = 3000;
// Docks refresh only once visible; this lets the deferred refresh run after showing one.
constexpr int dockLoadMs = 200;
constexpr std::chrono::milliseconds hangTimeout { 60000 };
// Menus nested deeper than this are generated lists (types, offsets), not more of the UI.
constexpr int maxMenuDepth = 4;

QtMessageHandler previousHandler = nullptr;

/** Drops the offscreen platform's warning for every popup and dock it can't fully support. */
void quietHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    if (message.startsWith("This plugin does not support")) {
        return;
    }
    previousHandler(type, context, message);
}

/**
 * Entries that end the test or change things outside it: a new Cutter process, quitting, a
 * browser, and writing to the fixture, which other tests share.
 */
const QSet<QString> deniedActions = { "actionNew",           "actionQuit",
                                      "actionIssue",         "actionDocumentation",
                                      "actionWriteMode",     "actionCommitChanges" };

bool tasksSettled()
{
    return !CutterTest::core()->getAsyncTaskManager()->getTasksRunning()
            && !CutterTest::core()->isDebugTaskInProgress();
}

bool isDenied(const QAction *action)
{
    if (deniedActions.contains(action->objectName())) {
        return true;
    }
    auto *debug = CutterTest::mainWindow()->findChild<DebugActions *>();
    // Emulation is started on purpose for the second pass.
    return debug && action == debug->actionStartEmul;
}

QString itemText(const QAction *action)
{
    QString text = action->text().remove('&').remove("...").trimmed();
    if (text.isEmpty()) {
        text = action->objectName().isEmpty() ? action->toolTip() : action->objectName();
    }
    return text;
}

/** The visible entries of a menu, keyed by text; repeated texts get " #2", " #3", ... */
QList<QPair<QString, QAction *>> menuItems(const QMenu *menu)
{
    QList<QPair<QString, QAction *>> items;
    QHash<QString, int> seen;
    for (QAction *action : menu->actions()) {
        if (action->isSeparator() || !action->isVisible()) {
            continue;
        }
        QString key = itemText(action);
        const int count = ++seen[key];
        if (count > 1) {
            key += QString(" #%1").arg(count);
        }
        items.append({ key, action });
    }
    return items;
}

void closePopups()
{
    while (QWidget *popup = QApplication::activePopupWidget()) {
        popup->close();
    }
}

bool openSubmenu(QMenu *menu, QAction *action)
{
    menu->setActiveAction(action);
    QMenu *submenu = action->menu();
    return QTest::qWaitFor([submenu]() { return submenu && submenu->isVisible(); },
                           menuTimeoutMs);
}

CutterDockWidget *owningDock(QObject *object)
{
    for (; object; object = object->parent()) {
        if (auto *dock = qobject_cast<CutterDockWidget *>(object)) {
            return dock;
        }
    }
    return nullptr;
}

/** Where a user would right-click or double-click: the first row, or the middle. */
QPoint clickPoint(QWidget *viewport)
{
    if (auto *view = qobject_cast<QAbstractItemView *>(viewport->parentWidget())) {
        QAbstractItemModel *model = view->model();
        if (model && model->rowCount() > 0) {
            return view->visualRect(model->index(0, 0)).center();
        }
    }
    return viewport->rect().center();
}

void closeOtherWindows(QWidget *main)
{
    closePopups();
    for (QWidget *window : QApplication::topLevelWidgets()) {
        if (window != main && window->isVisible() && !qobject_cast<QDockWidget *>(window)) {
            window->close();
        }
    }
}

/**
 * @brief Rejects every window that opens, except the menus the test is opening itself.
 *
 * Rejecting from a zero timer lets a dialog started with exec() enter its event loop first, so
 * exec() returns as if the user pressed Cancel. Progress dialogs are left alone: they close
 * when their task ends, and rejecting one cancels the task.
 *
 * expectMenu() hands the next menu that opens to a callback, run inside that menu's event loop
 * as a user's clicks would be. Submenus opened while the callback navigates are left open.
 * menuWasHandled() is true once the callback has returned.
 */
class Driver : public QObject
{
public:
    using MenuCallback = std::function<void(QMenu *)>;

    explicit Driver(QWidget *keep) : keep(keep) { qApp->installEventFilter(this); }
    ~Driver() override { qApp->removeEventFilter(this); }

    void expectMenu(MenuCallback callback)
    {
        onMenu = std::move(callback);
        menuHandled = false;
        navigating = false;
    }

    bool menuWasHandled() const { return menuHandled; }

    /** Stops leaving menus open: from here on, what the clicked entry opens is rejected. */
    void stopNavigating() { navigating = false; }

    void reset()
    {
        onMenu = nullptr;
        navigating = false;
    }

    QStringList takeSeen()
    {
        QStringList result = seen;
        seen.clear();
        return result;
    }

protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        auto *widget = qobject_cast<QWidget *>(object);
        if (event->type() != QEvent::Show || !widget || !widget->isWindow() || widget == keep
            || qobject_cast<QDockWidget *>(widget) || isProgressDialog(widget)) {
            return false;
        }
        if (auto *menu = qobject_cast<QMenu *>(widget)) {
            if (navigating) {
                return false;
            }
            if (onMenu) {
                navigating = true;
                const MenuCallback callback = std::exchange(onMenu, nullptr);
                const QPointer<QMenu> guard(menu);
                QTimer::singleShot(0, menu, [this, guard, callback]() {
                    if (guard) {
                        callback(guard);
                    }
                    navigating = false;
                    menuHandled = true;
                });
                return false;
            }
        }
        seen << QString::fromUtf8(widget->metaObject()->className());
        const QPointer<QWidget> guard(widget);
        QTimer::singleShot(0, widget, [guard]() {
            if (auto *dialog = qobject_cast<QDialog *>(guard.data())) {
                dialog->reject();
            } else if (guard) {
                guard->close();
            }
        });
        return false;
    }

private:
    static bool isProgressDialog(const QWidget *widget)
    {
        return widget->inherits("RizinTaskDialog") || widget->inherits("AsyncTaskDialog");
    }

    QWidget *keep;
    MenuCallback onMenu;
    bool menuHandled = false;
    bool navigating = false;
    QStringList seen;
};

struct Step
{
    enum Kind { MenuBar, ToolButton, ToolMenu, ContextMenu, Shortcut };

    Kind kind;
    QString name;
    QPointer<QWidget> widget;
    QPointer<QAction> action;
    QStringList path;
    // For steps inside a dock: the dock's name and the view's or shortcut's key in it, to find
    // them again if an earlier row (resetting the layout) recreated the dock.
    QString dock;
    QString key;
};

/** The dock's visible views by key: class name, with " #2", " #3", ... for repeats. */
QList<QPair<QString, QWidget *>> dockViews(CutterDockWidget *dock)
{
    QList<QPair<QString, QWidget *>> views;
    QHash<QString, int> seen;
    for (auto *area : dock->findChildren<QAbstractScrollArea *>()) {
        // Headers have no menus of their own, and an empty list has no row to right-click.
        auto *view = qobject_cast<QAbstractItemView *>(area);
        if (!area->isVisible() || qobject_cast<QHeaderView *>(area)
            || (view && (!view->model() || view->model()->rowCount() == 0))) {
            continue;
        }
        QString key = QString::fromUtf8(area->metaObject()->className());
        const int count = ++seen[key];
        if (count > 1) {
            key += QString(" #%1").arg(count);
        }
        views.append({ key, area->viewport() });
    }
    return views;
}

/** The dock's actions that are in no menu or toolbar but have a shortcut, by key. */
QList<QPair<QString, QAction *>> dockShortcuts(CutterDockWidget *dock)
{
    QList<QPair<QString, QAction *>> shortcuts;
    for (QAction *action : dock->findChildren<QAction *>()) {
        const QList<QObject *> owners = action->associatedObjects();
        const bool inMenu = std::any_of(owners.begin(), owners.end(), [](QObject *owner) {
            return qobject_cast<QMenu *>(owner) || qobject_cast<QToolBar *>(owner);
        });
        const bool inWidget = !owners.isEmpty() && qobject_cast<QWidget *>(owners.first());
        if (!inMenu && inWidget && !action->shortcut().isEmpty() && !isDenied(action)) {
            shortcuts.append({ "key " + action->shortcut().toString() + " " + itemText(action),
                               action });
        }
    }
    return shortcuts;
}

bool opensMenu(Step::Kind kind)
{
    return kind == Step::MenuBar || kind == Step::ToolMenu || kind == Step::ContextMenu;
}

} // namespace

class TestUiSweep : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void everyDockOpens_data();
    void everyDockOpens();
    void everyEntryRuns_data();
    void everyEntryRuns();
    void everyContextMenuOpens_data();
    void everyContextMenuOpens();
    void everyDockOpensWhileEmulating_data();
    void everyDockOpensWhileEmulating();

private:
    void addDockRows();
    void openDock(const QString &name);
    bool startEmulation();
    void showDock(CutterDockWidget *dock);

    QList<Step> collectSteps();
    void addMenuSteps(const Step &opener, QList<Step> &steps);
    void listMenu(QMenu *menu, const QStringList &prefix, QList<QStringList> &paths,
                  QSet<QMenu *> &open);
    bool openMenu(const Step &step);
    void addRows(const QList<Step> &steps);
    void runStep(const Step &original, QString &skipped);
    void findAgain(Step &step);
    void runRow(const QList<Step> &list);

    MainWindow *main = nullptr;
    RVA target = RVA_INVALID;
    std::unique_ptr<Driver> driver;
    QPointer<CutterDockWidget> shownDock;
    QList<Step> entrySteps;
    QList<Step> contextSteps;
};

void TestUiSweep::initTestCase()
{
    previousHandler = qInstallMessageHandler(quietHandler);
    main = CutterTest::mainWindow();
    QVERIFY(main);
    main->show();
    main->activateWindow();
    QTRY_VERIFY_WITH_TIMEOUT(tasksSettled(), settleTimeoutMs);

    const QList<FunctionDescription> functions = CutterTest::core()->getAllFunctions();
    QVERIFY2(!functions.isEmpty(), "the fixture has no functions");
    target = functions.first().offset;
    for (const FunctionDescription &function : functions) {
        if (function.name == "main" || function.name == "sym.main") {
            target = function.offset;
        }
    }
    driver = std::make_unique<Driver>(main);
}

void TestUiSweep::init()
{
    closeOtherWindows(main);
    driver->reset();
    driver->takeSeen();
    CutterTest::core()->seekAndShow(target);
    QTRY_VERIFY_WITH_TIMEOUT(tasksSettled(), settleTimeoutMs);
}

void TestUiSweep::cleanupTestCase()
{
    if (CutterTest::core()->currentlyDebugging) {
        CutterTest::core()->stopDebug();
    }
    driver.reset();
    qInstallMessageHandler(previousHandler);
}

/**
 * Shows the dock and lets it load; hides the dock shown before, so they aren't all alive.
 *
 * The dock may already be the shown one but hidden behind another tab: init() seeks, which
 * raises a memory widget over it. It is raised again then, or its views can't be clicked.
 */
void TestUiSweep::showDock(CutterDockWidget *dock)
{
    // Memory docks delete themselves when closed, so they are left open.
    if (shownDock && dock != shownDock && !qobject_cast<MemoryDockWidget *>(shownDock.data())) {
        shownDock->toggleDockWidget(false);
    }
    shownDock = dock;
    if (dock && !dock->isVisible()) {
        dock->toggleDockWidget(true);
        dock->raise();
        QTest::qWait(dockLoadMs);
        QTRY_VERIFY_WITH_TIMEOUT(tasksSettled(), settleTimeoutMs);
    }
}

/** Lists the menu's entries. open holds the menus above it, so a menu inside itself ends. */
void TestUiSweep::listMenu(QMenu *menu, const QStringList &prefix, QList<QStringList> &paths,
                           QSet<QMenu *> &open)
{
    open << menu;
    for (const auto &[key, action] : menuItems(menu)) {
        // A repeated entry is a generated list (like the seek history); its first one is enough.
        if (isDenied(action) || key != itemText(action)) {
            continue;
        }
        const QStringList path = prefix + QStringList { key };
        QMenu *submenu = action->menu();
        if (!submenu || prefix.size() + 1 >= maxMenuDepth || open.contains(submenu)) {
            paths << path;
        } else if (action->isEnabled() && openSubmenu(menu, action)) {
            listMenu(submenu, path, paths, open);
            submenu->close();
        }
    }
    open.remove(menu);
}

/** Opens the menu the way a user would. Returns false if no menu opened. */
bool TestUiSweep::openMenu(const Step &step)
{
    QWidget *widget = step.widget;
    if (!widget) {
        return false;
    }
    switch (step.kind) {
    case Step::MenuBar: {
        auto *bar = qobject_cast<QMenuBar *>(widget);
        if (!step.action) {
            return false;
        }
        QTest::mouseClick(bar, Qt::LeftButton, {}, bar->actionGeometry(step.action).center());
        break;
    }
    case Step::ToolMenu:
        static_cast<QToolButton *>(widget)->showMenu();
        break;
    case Step::ContextMenu: {
        const QPoint point = clickPoint(widget);
        QTest::mouseClick(widget, Qt::LeftButton, {}, point);
        QContextMenuEvent event(QContextMenuEvent::Mouse, point, widget->mapToGlobal(point));
        QApplication::sendEvent(widget, &event);
        break;
    }
    default:
        return false;
    }
    // exec() menus are done when sendEvent returns; popup() menus need the event loop.
    return QTest::qWaitFor([this]() { return driver->menuWasHandled(); }, menuTimeoutMs);
}

void TestUiSweep::addMenuSteps(const Step &opener, QList<Step> &steps)
{
    // Collection runs before any row, so a hang here would otherwise name nothing.
    qInfo().noquote() << "listing" << opener.name;
    const CutterTest::DeadlockWatchdog watchdog("listing " + opener.name.toUtf8(), hangTimeout);
    QList<QStringList> paths;
    driver->expectMenu([this, &paths](QMenu *menu) {
        QSet<QMenu *> open;
        listMenu(menu, {}, paths, open);
        closePopups();
    });
    if (!openMenu(opener)) {
        driver->reset();
        return;
    }
    QTRY_VERIFY_WITH_TIMEOUT(tasksSettled(), settleTimeoutMs);
    closeOtherWindows(main);
    for (const QStringList &path : paths) {
        Step step = opener;
        step.path = path;
        step.name = opener.name + "/" + path.join(" > ");
        steps << step;
    }
}

/**
 * Finds every menu-bar, toolbar and shortcut entry by doing what a user would: open each menu
 * and read it. Docks are shown one at a time, so only visible views get a context-menu step;
 * those go to contextSteps.
 */
QList<Step> TestUiSweep::collectSteps()
{
    QList<Step> found;
    contextSteps.clear();
    QMenuBar *bar = main->menuBar();
    for (QAction *action : bar->actions()) {
        if (action->menu() && action->isVisible()) {
            addMenuSteps({ Step::MenuBar, "Menu/" + itemText(action), bar, action, {} }, found);
        }
    }

    for (QToolBar *toolBar : main->findChildren<QToolBar *>()) {
        showDock(owningDock(toolBar));
        if (!toolBar->isVisible()) {
            continue;
        }
        const QString prefix = "Toolbar/" + toolBar->objectName() + "/";
        for (QAction *action : toolBar->actions()) {
            auto *button = qobject_cast<QToolButton *>(toolBar->widgetForAction(action));
            if (!button || isDenied(action)) {
                continue;
            }
            const QString name = prefix + itemText(action);
            if (button->menu() || action->menu()) {
                addMenuSteps({ Step::ToolMenu, name, button, action, {} }, found);
            }
            if (button->popupMode() != QToolButton::InstantPopup || !button->menu()) {
                found << Step { Step::ToolButton, name, button, action, {} };
            }
        }
    }

    for (CutterDockWidget *dock : main->findChildren<CutterDockWidget *>()) {
        showDock(dock);
        const QString prefix = "Dock/" + dock->objectName() + "/";
        for (const auto &[key, viewport] : dockViews(dock)) {
            contextSteps << Step { Step::ContextMenu, prefix + key, viewport, {}, {},
                                   dock->objectName(), key };
        }
        // Shortcuts that are in no menu or toolbar: only the key reaches them.
        for (const auto &[key, action] : dockShortcuts(dock)) {
            auto *widget = qobject_cast<QWidget *>(action->associatedObjects().first());
            found << Step { Step::Shortcut, prefix + key, widget, action, {},
                            dock->objectName(), key };
        }
    }
    showDock(nullptr);
    return found;
}

/** Finds the step's view or shortcut in the dock of the same name, if the dock was recreated. */
void TestUiSweep::findAgain(Step &step)
{
    auto *dock = main->findChild<CutterDockWidget *>(step.dock);
    if (!dock) {
        return;
    }
    showDock(dock);
    if (step.kind == Step::Shortcut) {
        for (const auto &[key, action] : dockShortcuts(dock)) {
            if (key == step.key) {
                step.action = action;
                step.widget = qobject_cast<QWidget *>(action->associatedObjects().first());
            }
        }
        return;
    }
    for (const auto &[key, viewport] : dockViews(dock)) {
        if (key == step.key) {
            step.widget = viewport;
        }
    }
}

void TestUiSweep::addRows(const QList<Step> &list)
{
    QTest::addColumn<int>("index");
    QHash<QString, int> seen;
    for (int index = 0; index < list.size(); ++index) {
        QString name = list[index].name;
        const int count = ++seen[name];
        if (count > 1) {
            name += QString(" #%1").arg(count);
        }
        QTest::newRow(qPrintable(name)) << index;
    }
}

/**
 * Does the step as a user would: shows the dock it belongs to, opens the menu and clicks the
 * entry (or only opens it), or clicks the button, presses the key.
 *
 * Sets skipped to why the step couldn't run; leaves it empty if it ran.
 */
void TestUiSweep::runStep(const Step &original, QString &skipped)
{
    Step step = original;
    if (!step.widget && !step.dock.isEmpty()) {
        findAgain(step);
    }
    QWidget *widget = step.widget;
    if (!widget) {
        skipped = "the widget no longer exists";
        return;
    }
    showDock(owningDock(widget));
    if (QTest::currentTestFailed()) {
        return;
    }

    enum class Outcome { NoMenu, Missing, Disabled, Clicked } outcome = Outcome::NoMenu;
    // A checkable entry (lock panels, tabs on top) is clicked again afterwards, so its state
    // doesn't carry over and disable later rows.
    QPointer<QAction> clicked;
    bool wasChecked = false;
    const QList<CutterDockWidget *> docksBefore = main->findChildren<CutterDockWidget *>();
    {
        const CutterTest::DeadlockWatchdog watchdog(QTest::currentDataTag(), hangTimeout);
        // A crash kills the process before the row is reported; this names it.
        qInfo().noquote() << "running" << QTest::currentDataTag();

        if (opensMenu(step.kind)) {
            driver->expectMenu([this, &step, &outcome, &clicked, &wasChecked](QMenu *menu) {
                if (step.path.isEmpty()) {
                    outcome = Outcome::Clicked;
                }
                for (int i = 0; i < step.path.size(); ++i) {
                    QAction *action = nullptr;
                    for (const auto &[key, item] : menuItems(menu)) {
                        if (key == step.path[i]) {
                            action = item;
                        }
                    }
                    if (!action) {
                        outcome = Outcome::Missing;
                        break;
                    }
                    if (!action->isEnabled()) {
                        outcome = Outcome::Disabled;
                        break;
                    }
                    if (i + 1 < step.path.size()) {
                        if (!openSubmenu(menu, action)) {
                            outcome = Outcome::Missing;
                            break;
                        }
                        menu = action->menu();
                        continue;
                    }
                    outcome = Outcome::Clicked;
                    clicked = action;
                    wasChecked = action->isChecked();
                    driver->stopNavigating();
                    QTest::mouseClick(menu, Qt::LeftButton, {},
                                      menu->actionGeometry(action).center());
                    return;
                }
                closePopups();
            });
            openMenu(step);
            driver->reset();
        } else if (step.kind == Step::ToolButton) {
            if (step.action && !step.action->isEnabled()) {
                outcome = Outcome::Disabled;
            } else {
                outcome = Outcome::Clicked;
                clicked = step.action;
                wasChecked = clicked && clicked->isChecked();
                QTest::mouseClick(widget, Qt::LeftButton);
            }
        } else if (step.kind == Step::Shortcut) {
            if (!step.action || !step.action->isEnabled()) {
                outcome = Outcome::Disabled;
            } else {
                outcome = Outcome::Clicked;
                clicked = step.action;
                wasChecked = clicked->isChecked();
                main->activateWindow();
                widget->setFocus(Qt::ShortcutFocusReason);
                QTest::keySequence(widget, step.action->shortcut());
            }
        }
        QTRY_VERIFY_WITH_TIMEOUT(tasksSettled(), settleTimeoutMs);
    }

    if (clicked && clicked->isCheckable() && clicked->isChecked() != wasChecked) {
        clicked->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(tasksSettled(), settleTimeoutMs);
    }
    const QStringList opened = driver->takeSeen();
    if (!opened.isEmpty()) {
        qInfo() << "opened" << opened;
    }
    for (CutterDockWidget *added : main->findChildren<CutterDockWidget *>()) {
        if (!docksBefore.contains(added)) {
            added->close();
        }
    }
    closeOtherWindows(main);

    switch (outcome) {
    case Outcome::NoMenu:
        skipped = step.kind == Step::ContextMenu ? "the view has no context menu"
                                                 : "the menu didn't open";
        break;
    case Outcome::Missing:
        skipped = "the entry isn't in the menu in this state";
        break;
    case Outcome::Disabled:
        skipped = "disabled in this state";
        break;
    case Outcome::Clicked:
        break;
    }
}

void TestUiSweep::addDockRows()
{
    QTest::addColumn<QString>("dockName");
    QSet<QString> names;
    for (CutterDockWidget *dock : main->findChildren<CutterDockWidget *>()) {
        if (!names.contains(dock->objectName())) {
            names << dock->objectName();
            QTest::newRow(qPrintable(dock->objectName())) << dock->objectName();
        }
    }
}

/** Shows the dock, seeks elsewhere and back while it is visible, then hides it again. */
void TestUiSweep::openDock(const QString &name)
{
    auto *dock = main->findChild<CutterDockWidget *>(name);
    if (!dock) {
        QSKIP("the dock no longer exists");
    }
    const CutterTest::DeadlockWatchdog watchdog(name.toUtf8(), hangTimeout);
    showDock(dock);
    CutterTest::core()->seekAndShow(target + 1);
    CutterTest::core()->seekAndShow(target);
    QTRY_VERIFY_WITH_TIMEOUT(tasksSettled(), settleTimeoutMs);
    showDock(nullptr);
}

bool TestUiSweep::startEmulation()
{
    CutterCore *core = CutterTest::core();
    if (!core->currentlyEmulating) {
        auto *debug = main->findChild<DebugActions *>();
        if (!debug) {
            return false;
        }
        debug->actionStartEmul->trigger();
    }
    return QTest::qWaitFor([core]() { return core->currentlyEmulating && tasksSettled(); },
                           settleTimeoutMs);
}

void TestUiSweep::everyDockOpens_data()
{
    addDockRows();
}

void TestUiSweep::everyDockOpens()
{
    QFETCH(const QString, dockName);
    openDock(dockName);
}

void TestUiSweep::everyEntryRuns_data()
{
    entrySteps = collectSteps();
    addRows(entrySteps);
}

void TestUiSweep::everyEntryRuns()
{
    runRow(entrySteps);
}

/** The views are found while collecting the entries, which normally runs first. */
void TestUiSweep::everyContextMenuOpens_data()
{
    if (contextSteps.isEmpty()) {
        entrySteps = collectSteps();
    }
    addRows(contextSteps);
}

void TestUiSweep::everyContextMenuOpens()
{
    runRow(contextSteps);
}

void TestUiSweep::runRow(const QList<Step> &list)
{
    QFETCH(const int, index);
    QString skipped;
    runStep(list[index], skipped);
    if (QTest::currentTestFailed()) {
        return;
    }
    if (!skipped.isEmpty()) {
        QSKIP(qPrintable(skipped));
    }
}

void TestUiSweep::everyDockOpensWhileEmulating_data()
{
    addDockRows();
}

void TestUiSweep::everyDockOpensWhileEmulating()
{
    QFETCH(const QString, dockName);
    QVERIFY2(startEmulation(), "emulation didn't start");
    openDock(dockName);
}

CUTTER_APP_TEST_MAIN_WITH_FILE(TestUiSweep, CutterTest::Fixtures::helloX64())
#include "test_UiSweep.moc"
