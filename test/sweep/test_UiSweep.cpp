/**
 * @file test_UiSweep.cpp
 * @brief Every path a user can take through the main window runs without crashing or hanging.
 *
 * The test crawls the UI as a user would and nothing is listed by hand. It clicks every
 * menu-bar entry (opening submenus by hovering), every toolbar button and dropdown entry, every
 * entry of every dock's context menus (right-clicking each view), every button and dropdown in
 * every dock, every value of every combo box in a dock, double-clicks every list and presses
 * every shortcut that isn't in a menu. All of it is found at run time, so new widgets, menus,
 * dialogs and entries are covered without editing this file.
 *
 * The first time a window of a class opens, the crawler explores it before cancelling it: it
 * clicks every tab, the first rows of every list (the pages of Preferences, for example) and
 * every button except those that commit or delete (OK, Apply, Save, Delete, ...). What those
 * buttons open is explored the same way. Qt's own dialogs (file, font, color, message boxes)
 * are only cancelled. Later openings of an explored class are cancelled straight away.
 *
 * Every dock opens again under ESIL emulation, and everything that was disabled or missing
 * runs again there. A debugger run still going after a few seconds is suspended, as a user
 * would. Entries that would leave the test or touch the fixture are in
 * deniedActions; buttons that commit or delete match deniedButtons.
 */

#include "AsyncTask.h"
#include "CutterDockWidget.h"
#include "DeadlockWatchdog.h"
#include "DebugActions.h"
#include "Fixtures.h"
#include "MemoryDockWidget.h"
#include "TestEnv.h"
#include "core/MainWindow.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHash>
#include <QHeaderView>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSet>
#include <QTabBar>
#include <QTabWidget>
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
// A row can explore several new dialogs, each with pages and buttons.
constexpr std::chrono::milliseconds hangTimeout { 180000 };
// Menus nested deeper than this are generated lists (types, offsets), not more of the UI.
constexpr int maxMenuDepth = 4;
// Rows of a list a dialog explores, and values of a combo box a dock tries; the rest are more
// of the same.
constexpr int maxExploredRows = 20;
constexpr int maxComboValues = 50;
// A run (Continue, Continue until) can go on forever under emulation; after this long the
// crawler presses Suspend, as a user would.
constexpr int debugRunMs = 3000;

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

/** Buttons that commit, delete or close; exploring a window must not change anything. */
const QRegularExpression deniedButtons(
        R"(^(ok|apply|save|delete|remove|reset|restore|clear|yes|accept|done|close|cancel|)"
        R"(quit|exit|commit|overwrite|default|purge)\b)",
        QRegularExpression::CaseInsensitiveOption);

bool tasksSettled()
{
    return !CutterTest::core()->getAsyncTaskManager()->getTasksRunning()
            && !CutterTest::core()->isDebugTaskInProgress();
}

/** Presses Continue while the debugger runs, which is Suspend then. */
void suspendDebugging()
{
    auto *debug = CutterTest::mainWindow()->findChild<DebugActions *>();
    if (debug && debug->actionContinue->isEnabled()) {
        qInfo().noquote() << "  still running after" << debugRunMs << "ms, suspending";
        debug->actionContinue->trigger();
    }
}

/**
 * Lets what a click queued run (deferred refreshes, docks loading once visible), then waits
 * only while Cutter's background tasks are running. A debugger run that goes on is suspended.
 */
bool settle()
{
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    if (!QTest::qWaitFor([]() { return !CutterTest::core()->isDebugTaskInProgress(); },
                         debugRunMs)) {
        suspendDebugging();
    }
    return QTest::qWaitFor(tasksSettled, settleTimeoutMs);
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

QString cleanText(QString text)
{
    return text.remove('&').remove("...").trimmed();
}

QString itemText(const QAction *action)
{
    QString text = cleanText(action->text());
    if (text.isEmpty()) {
        text = action->objectName().isEmpty() ? action->toolTip() : action->objectName();
    }
    return text;
}

QString buttonText(const QAbstractButton *button)
{
    QString text = cleanText(button->text());
    if (text.isEmpty()) {
        text = cleanText(button->toolTip());
    }
    if (text.isEmpty()) {
        text = button->objectName();
    }
    return text;
}

template<typename T>
bool hasAncestor(const QObject *object)
{
    for (QObject *parent = object->parent(); parent; parent = parent->parent()) {
        if (qobject_cast<T *>(parent)) {
            return true;
        }
    }
    return false;
}

/** Children as QPointers: clicking one can delete others. */
template<typename T>
QList<QPointer<T>> guardedChildren(const QWidget *parent)
{
    QList<QPointer<T>> children;
    for (T *child : parent->findChildren<T *>()) {
        children << QPointer<T>(child);
    }
    return children;
}

/**
 * Buttons a user would press in a dock or dialog: visible, not Qt's own (title bars, tab
 * scrollers), not in a toolbar (collected separately) and not committing or deleting.
 */
bool isPressable(const QAbstractButton *button)
{
    return button->isVisible() && !button->objectName().startsWith("qt_")
            && !hasAncestor<QToolBar>(button) && !hasAncestor<QTabBar>(button)
            && !hasAncestor<QDialogButtonBox>(button)
            && !deniedButtons.match(buttonText(button)).hasMatch();
}

QMenu *buttonMenu(const QAbstractButton *button)
{
    if (auto *tool = qobject_cast<const QToolButton *>(button)) {
        return tool->menu() ? tool->menu()
                            : (tool->defaultAction() ? tool->defaultAction()->menu() : nullptr);
    }
    if (auto *push = qobject_cast<const QPushButton *>(button)) {
        return push->menu();
    }
    return nullptr;
}

/**
 * Clicks a button as a user would. A checkable one is clicked again if that changed its state,
 * so the state doesn't carry over into later rows (radio buttons can't be clicked back).
 */
void clickButton(QAbstractButton *button)
{
    const QPointer<QAbstractButton> guard(button);
    const bool wasChecked = button->isChecked();
    QTest::mouseClick(button, Qt::LeftButton);
    settle();
    if (guard && guard->isCheckable() && guard->isChecked() != wasChecked && guard->isEnabled()
        && guard->isVisible() && !qobject_cast<QRadioButton *>(guard.data())) {
        QTest::mouseClick(guard, Qt::LeftButton);
        settle();
    }
}

/**
 * Visits everything in a window a user can look at without committing: every tab, the first
 * rows of every list, and every pressable button. What those open is handled by the Driver.
 */
void exploreWindow(QWidget *window)
{
    const QPointer<QWidget> guard(window);
    for (const QPointer<QTabWidget> &tabs : guardedChildren<QTabWidget>(window)) {
        for (int i = 0; guard && tabs && i < tabs->count(); ++i) {
            if (tabs->isTabEnabled(i) && tabs->tabBar()->isVisible()) {
                QTest::mouseClick(tabs->tabBar(), Qt::LeftButton, {},
                                  tabs->tabBar()->tabRect(i).center());
                settle();
            }
        }
    }
    for (const QPointer<QAbstractItemView> &view : guardedChildren<QAbstractItemView>(window)) {
        if (!guard || !view || !view->isVisible() || !view->model()
            || qobject_cast<QHeaderView *>(view.data())) {
            continue;
        }
        const int rows = std::min(view->model()->rowCount(), maxExploredRows);
        for (int row = 0; guard && view && row < rows; ++row) {
            const QModelIndex index = view->model()->index(row, 0);
            view->scrollTo(index);
            const QRect rect = view->visualRect(index);
            if (!rect.isEmpty()) {
                QTest::mouseClick(view->viewport(), Qt::LeftButton, {}, rect.center());
                settle();
            }
        }
    }
    for (const QPointer<QAbstractButton> &button : guardedChildren<QAbstractButton>(window)) {
        if (!guard) {
            return;
        }
        if (button && button->isEnabled() && isPressable(button)) {
            qInfo().noquote() << "  pressing" << buttonText(button);
            clickButton(button);
        }
    }
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

/**
 * The entry's text without the state it shows: a trailing "(Empty)" or " (sym.main)",
 * addresses ("Add flag at 0x00000138") and quoted names ("Edit function \"entry0\"").
 */
QString withoutState(const QString &key)
{
    static const QRegularExpression state(R"(\s*\([^()]*\)$)");
    static const QRegularExpression address("0x[0-9a-fA-F]+");
    static const QRegularExpression quoted(R"("[^"]*")");
    return QString(key).remove(state).replace(address, "0x").replace(quoted, "\"\"");
}

/**
 * The entry with this key. Some entries show state in their text ("Graph(Empty)" until the
 * graph has a function, the address under the cursor); if the text changed since the menu was
 * read, the entry with the same text without that state is used.
 */
QAction *findEntry(const QMenu *menu, const QString &key)
{
    const QList<QPair<QString, QAction *>> items = menuItems(menu);
    for (const auto &[text, action] : items) {
        if (text == key) {
            return action;
        }
    }
    for (const auto &[text, action] : items) {
        if (withoutState(text) == withoutState(key)) {
            return action;
        }
    }
    return nullptr;
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
 * @brief Handles every window that opens, except the menus the test is opening itself.
 *
 * The first window of each Cutter class is explored (exploreWindow()), then cancelled; later
 * ones and Qt's own dialogs are cancelled straight away. Both run from a zero timer, so a
 * dialog started with exec() is in its event loop and exec() returns as if the user pressed
 * Cancel. Progress dialogs are left alone: they close when their task ends, and rejecting one
 * cancels the task.
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

    /** Stops leaving menus open: from here on, what the clicked entry opens is handled. */
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

    QStringList exploredClasses() const
    {
        QStringList result(explored.begin(), explored.end());
        result.sort();
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
        const QString name = QString::fromUtf8(widget->metaObject()->className());
        seen << name;
        // Qt's own classes start with Q; Cutter's don't.
        const bool explore = !qobject_cast<QMenu *>(widget) && !name.startsWith('Q')
                && !explored.contains(name);
        if (explore) {
            explored << name;
        }
        const QPointer<QWidget> guard(widget);
        QTimer::singleShot(0, widget, [guard, explore, name]() {
            if (explore && guard) {
                qInfo().noquote() << "exploring" << name;
                exploreWindow(guard);
            }
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
    QSet<QString> explored;
};

/** One thing a user does. Menu kinds open a menu, then click the entry at path. */
struct Step
{
    enum Kind { MenuBar, ToolButton, ToolMenu, ContextMenu, Shortcut, Button, Combo, DoubleClick };

    Kind kind;
    QString name;
    QPointer<QWidget> widget;
    QPointer<QAction> action;
    QStringList path;
    // For steps inside a dock: the dock's name and the widget's key in it, to find them again
    // if an earlier row (resetting the layout) recreated the dock.
    QString dock;
    QString key;
};

using Keyed = QList<QPair<QString, QWidget *>>;

/** Adds " #2", " #3", ... to repeated keys. */
void addKeyed(Keyed &list, QHash<QString, int> &seen, QString key, QWidget *widget)
{
    const int count = ++seen[key];
    if (count > 1) {
        key += QString(" #%1").arg(count);
    }
    list.append({ key, widget });
}

/** The dock's visible views by class name, as viewports. Empty lists have nothing to click. */
Keyed dockViews(CutterDockWidget *dock)
{
    Keyed views;
    QHash<QString, int> seen;
    for (auto *area : dock->findChildren<QAbstractScrollArea *>()) {
        auto *view = qobject_cast<QAbstractItemView *>(area);
        if (!area->isVisible() || qobject_cast<QHeaderView *>(area)
            || (view && (!view->model() || view->model()->rowCount() == 0))) {
            continue;
        }
        addKeyed(views, seen, QString::fromUtf8(area->metaObject()->className()),
                 area->viewport());
    }
    return views;
}

/** The dock's own buttons (not its toolbars'), by text. */
Keyed dockButtons(CutterDockWidget *dock)
{
    Keyed buttons;
    QHash<QString, int> seen;
    for (auto *button : dock->findChildren<QAbstractButton *>()) {
        if (isPressable(button)) {
            addKeyed(buttons, seen, "button " + buttonText(button), button);
        }
    }
    return buttons;
}

Keyed dockCombos(CutterDockWidget *dock)
{
    Keyed combos;
    QHash<QString, int> seen;
    for (auto *combo : dock->findChildren<QComboBox *>()) {
        if (combo->isVisible() && combo->count() > 1 && !hasAncestor<QToolBar>(combo)) {
            const QString name = combo->objectName().isEmpty() ? QStringLiteral("combo")
                                                                : combo->objectName();
            addKeyed(combos, seen, "combo " + name, combo);
        }
    }
    return combos;
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
    void everyContextEntryRuns_data();
    void everyContextEntryRuns();
    void everyDockOpensWhileEmulating_data();
    void everyDockOpensWhileEmulating();
    void everythingRunsWhileEmulating_data();
    void everythingRunsWhileEmulating();

private:
    void addDockRows();
    void openDock(const QString &name);
    bool startEmulation();
    void showDock(CutterDockWidget *dock);

    void collectSteps();
    void addMenuSteps(const Step &opener, QList<Step> &steps, bool dedupe = false);
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
    QList<Step> emulationSteps;
    QSet<QString> menuSignatures;
    QSet<QString> ran;
};

void TestUiSweep::initTestCase()
{
    previousHandler = qInstallMessageHandler(quietHandler);
    main = CutterTest::mainWindow();
    QVERIFY(main);
    main->show();
    main->activateWindow();
    QVERIFY(settle());

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
    if (CutterTest::core()->getOffset() != target) {
        CutterTest::core()->seek(target);
    }
    QVERIFY(settle());
}

void TestUiSweep::cleanupTestCase()
{
    if (driver) {
        qInfo().noquote() << "explored" << driver->exploredClasses().join(", ");
    }
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
 * can raise a memory widget over it. It is raised again then, or its views can't be clicked.
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
        QVERIFY(settle());
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
        if (auto *tool = qobject_cast<QToolButton *>(widget)) {
            tool->showMenu();
        } else if (auto *push = qobject_cast<QPushButton *>(widget)) {
            push->showMenu();
        }
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
    // exec() menus are done when the call returns; popup() menus need the event loop.
    return QTest::qWaitFor([this]() { return driver->menuWasHandled(); }, menuTimeoutMs);
}

/**
 * Opens the menu, reads its entries and adds a step per entry. With dedupe, a menu with the
 * same entries as one already read in a dock of the same class adds nothing.
 */
void TestUiSweep::addMenuSteps(const Step &opener, QList<Step> &steps, bool dedupe)
{
    // Collection runs before any row, so a hang here would otherwise name nothing.
    qInfo().noquote() << "listing" << opener.name;
    const CutterTest::DeadlockWatchdog watchdog("listing " + opener.name.toUtf8(), hangTimeout);
    QList<QStringList> paths;
    QString menuClass;
    driver->expectMenu([this, &paths, &menuClass](QMenu *menu) {
        menuClass = QString::fromUtf8(menu->metaObject()->className());
        QSet<QMenu *> open;
        listMenu(menu, {}, paths, open);
        closePopups();
    });
    if (!openMenu(opener)) {
        driver->reset();
        return;
    }
    QVERIFY(settle());
    closeOtherWindows(main);
    if (dedupe) {
        const CutterDockWidget *dock = owningDock(opener.widget.data());
        QStringList entries;
        for (const QStringList &path : paths) {
            entries << path.join(" > ");
        }
        const QString signature =
                QString::fromUtf8(dock ? dock->metaObject()->className() : "") + "|"
                + menuClass + "|" + entries.join("|");
        if (menuSignatures.contains(signature)) {
            qInfo().noquote() << "  same menu as an earlier view";
            return;
        }
        menuSignatures << signature;
    }
    for (const QStringList &path : paths) {
        Step step = opener;
        step.path = path;
        step.name = opener.name + "/" + path.join(" > ");
        steps << step;
    }
}

/**
 * Finds everything a user can do by doing what a user would: opening each menu and reading it,
 * and showing each dock to look at what's in it. Context-menu entries go to contextSteps, the
 * rest to entrySteps.
 */
void TestUiSweep::collectSteps()
{
    entrySteps.clear();
    contextSteps.clear();
    menuSignatures.clear();
    QMenuBar *bar = main->menuBar();
    for (QAction *action : bar->actions()) {
        if (action->menu() && action->isVisible()) {
            addMenuSteps({ Step::MenuBar, "Menu/" + itemText(action), bar, action, {} },
                         entrySteps);
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
                addMenuSteps({ Step::ToolMenu, name, button, action, {} }, entrySteps);
            }
            if (button->popupMode() != QToolButton::InstantPopup || !button->menu()) {
                entrySteps << Step { Step::ToolButton, name, button, action, {} };
            }
        }
    }

    for (CutterDockWidget *dock : main->findChildren<CutterDockWidget *>()) {
        showDock(dock);
        const QString prefix = "Dock/" + dock->objectName() + "/";
        const QString dockName = dock->objectName();
        for (const auto &[key, viewport] : dockViews(dock)) {
            addMenuSteps({ Step::ContextMenu, prefix + key, viewport, {}, {}, dockName, key },
                         contextSteps, true);
            if (qobject_cast<QAbstractItemView *>(viewport->parentWidget())) {
                entrySteps << Step { Step::DoubleClick, prefix + key + "/double-click",
                                     viewport,          {},
                                     {},                dockName,
                                     key };
            }
        }
        for (const auto &[key, widget] : dockButtons(dock)) {
            auto *button = static_cast<QAbstractButton *>(widget);
            if (buttonMenu(button)) {
                addMenuSteps({ Step::ToolMenu, prefix + key, button, {}, {}, dockName, key },
                             entrySteps);
            }
            auto *tool = qobject_cast<QToolButton *>(button);
            const bool onlyMenu = qobject_cast<QPushButton *>(button)
                    ? buttonMenu(button) != nullptr
                    : tool && tool->popupMode() == QToolButton::InstantPopup
                            && buttonMenu(button);
            if (!onlyMenu) {
                entrySteps << Step { Step::Button, prefix + key, button, {}, {}, dockName, key };
            }
        }
        for (const auto &[key, widget] : dockCombos(dock)) {
            entrySteps << Step { Step::Combo, prefix + key, widget, {}, {}, dockName, key };
        }
        // Shortcuts that are in no menu or toolbar: only the key reaches them.
        for (const auto &[key, action] : dockShortcuts(dock)) {
            auto *widget = qobject_cast<QWidget *>(action->associatedObjects().first());
            entrySteps << Step { Step::Shortcut, prefix + key, widget, action, {},
                                 dockName,       key };
        }
    }
    showDock(nullptr);
}

/** Finds the step's widget in the dock of the same name, if the dock was recreated. */
void TestUiSweep::findAgain(Step &step)
{
    auto *dock = main->findChild<CutterDockWidget *>(step.dock);
    if (!dock) {
        return;
    }
    showDock(dock);
    if (step.kind == Step::Shortcut) {
        for (const auto &[key, action] : dockShortcuts(dock)) {
            if (withoutState(key) == withoutState(step.key)) {
                step.action = action;
                step.widget = qobject_cast<QWidget *>(action->associatedObjects().first());
            }
        }
        return;
    }
    Keyed candidates;
    if (step.kind == Step::ContextMenu || step.kind == Step::DoubleClick) {
        candidates = dockViews(dock);
    } else if (step.kind == Step::Combo) {
        candidates = dockCombos(dock);
    } else {
        candidates = dockButtons(dock);
    }
    for (const auto &[key, widget] : candidates) {
        if (withoutState(key) == withoutState(step.key)) {
            step.widget = widget;
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
 * entry, clicks the button, tries every value of the combo box, presses the key, or
 * double-clicks the row.
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
    QSet<QString> docksBefore;
    for (const CutterDockWidget *dock : main->findChildren<CutterDockWidget *>()) {
        docksBefore << dock->objectName();
    }
    {
        const CutterTest::DeadlockWatchdog watchdog(QTest::currentDataTag(), hangTimeout);
        // A crash kills the process before the row is reported; this names it.
        qInfo().noquote() << "running" << QTest::currentDataTag();

        if (opensMenu(step.kind)) {
            driver->expectMenu([this, &step, &outcome, &clicked, &wasChecked](QMenu *menu) {
                for (int i = 0; i < step.path.size(); ++i) {
                    QAction *action = findEntry(menu, step.path[i]);
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
        } else if (step.kind == Step::ToolButton || step.kind == Step::Button) {
            const bool enabled = step.action ? step.action->isEnabled() : widget->isEnabled();
            if (!enabled) {
                outcome = Outcome::Disabled;
            } else if (step.kind == Step::Button) {
                outcome = Outcome::Clicked;
                clickButton(static_cast<QAbstractButton *>(widget));
            } else {
                outcome = Outcome::Clicked;
                clicked = step.action;
                wasChecked = clicked && clicked->isChecked();
                QTest::mouseClick(widget, Qt::LeftButton);
            }
        } else if (step.kind == Step::Combo) {
            auto *combo = static_cast<QComboBox *>(widget);
            if (!combo->isEnabled()) {
                outcome = Outcome::Disabled;
            } else {
                // The keyboard changes the value without the popup, as a focused user would.
                outcome = Outcome::Clicked;
                const QPointer<QComboBox> guard(combo);
                const int original = combo->currentIndex();
                combo->setFocus(Qt::OtherFocusReason);
                QTest::keyClick(combo, Qt::Key_Home);
                QVERIFY(settle());
                const int values = std::min(combo->count(), maxComboValues);
                for (int i = 1; guard && i < values; ++i) {
                    QTest::keyClick(guard, Qt::Key_Down);
                    QVERIFY(settle());
                }
                if (guard) {
                    guard->setCurrentIndex(original);
                }
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
        } else if (step.kind == Step::DoubleClick) {
            outcome = Outcome::Clicked;
            QTest::mouseDClick(widget, Qt::LeftButton, {}, clickPoint(widget));
        }
        QVERIFY(settle());
    }

    if (clicked && clicked->isCheckable() && clicked->isChecked() != wasChecked) {
        clicked->trigger();
        QVERIFY(settle());
    }
    const QStringList opened = driver->takeSeen();
    if (!opened.isEmpty()) {
        qInfo() << "opened" << opened;
    }
    for (CutterDockWidget *added : main->findChildren<CutterDockWidget *>()) {
        // Only docks the row added; ones it replaced (resetting the layout recreates the
        // memory docks under the same names) are the layout now, and closing one deletes it.
        if (!docksBefore.contains(added->objectName())) {
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
    ran << list[index].name + "/" + QString::number(list[index].kind);
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
    QVERIFY(settle());
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
    collectSteps();
    addRows(entrySteps);
}

void TestUiSweep::everyEntryRuns()
{
    runRow(entrySteps);
}

/** The context menus are read while collecting the entries, which normally runs first. */
void TestUiSweep::everyContextEntryRuns_data()
{
    if (entrySteps.isEmpty()) {
        collectSteps();
    }
    addRows(contextSteps);
}

void TestUiSweep::everyContextEntryRuns()
{
    runRow(contextSteps);
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

/** Collected again while emulating: debugging enables entries and fills the debug docks. */
void TestUiSweep::everythingRunsWhileEmulating_data()
{
    startEmulation();
    collectSteps();
    emulationSteps.clear();
    for (const QList<Step> *list : { &entrySteps, &contextSteps }) {
        for (const Step &step : *list) {
            if (!ran.contains(step.name + "/" + QString::number(step.kind))) {
                emulationSteps << step;
            }
        }
    }
    addRows(emulationSteps);
}

void TestUiSweep::everythingRunsWhileEmulating()
{
    QVERIFY2(startEmulation(), "emulation didn't start");
    runRow(emulationSteps);
}

CUTTER_APP_TEST_MAIN_WITH_FILE(TestUiSweep, CutterTest::Fixtures::helloX64())
#include "test_UiSweep.moc"
