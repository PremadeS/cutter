/**
 * @file test_UiSweep.cpp
 * @brief Every path a user can take, through the main window, should run without crashing or
 * hanging.
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
#include <QButtonGroup>
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
#include <QShortcut>
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

/**
 * TODO: maybe make this global inside the common?
 * Drops the offscreen platform's warning for every popup and dock it can't fully support.
 */
void quietHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    if (message.startsWith("This plugin does not support")) {
        return;
    }
    previousHandler(type, context, message);
}

/**
 * Entries that end the test or change things outside it. e.g: a new Cutter process, quitting, a
 * browser, and writing to the fixture.
 */
const QSet<QString> deniedActions = { "actionNew",           "actionQuit",
                                      "actionIssue",         "actionDocumentation",
                                      "actionWriteMode",     "actionCommitChanges" };

/**
 * Buttons that commit, delete or close. Exploring a window must not change anything.
 */
const QRegularExpression deniedButtons(
        R"(^(ok|apply|save|delete|remove|reset|restore|clear|yes|accept|done|close|cancel|)"
        R"(quit|exit|commit|overwrite|default|purge)\b)",
        QRegularExpression::CaseInsensitiveOption);

bool tasksSettled()
{
    return !CutterTest::core()->getAsyncTaskManager()->getTasksRunning()
            && !CutterTest::core()->isDebugTaskInProgress();
}

/**
 * Ends a debug session by pressesing "Continue" while the debugger runs, which is "Suspend" by
 * then.
 */
void suspendDebugging()
{
    auto *debug = CutterTest::mainWindow()->findChild<DebugActions *>();
    if (debug && debug->actionContinue->isEnabled()) {
        qInfo().noquote() << "  still running after" << debugRunMs << "ms, suspending";
        debug->actionContinue->trigger();
    }
}

/**
 * @brief Wait for all background tasks to finish.
 *
 * returns early if backgroud tasks take longer than settleTimeoutMs
 */
bool settle()
{
    // run twice, a queued call (like a singleShot timer) can queue another one that starts
    // a task, and that task should be running before we wait on it
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    if (!QTest::qWaitFor([]() { return !CutterTest::core()->isDebugTaskInProgress(); },
                         debugRunMs)) {
        suspendDebugging();
    }
    return QTest::qWaitFor(tasksSettled, settleTimeoutMs);
}

/**
 * @brief Whether the sweep must not trigger action.
 */
bool isDenied(const QAction *action)
{
    if (deniedActions.contains(action->objectName())) {
        return true;
    }
    auto *debug = CutterTest::mainWindow()->findChild<DebugActions *>();

    // emulation is started manually for the second pass.
    return debug && action == debug->actionStartEmul;
}

/**
 * @brief Text without mnemonics, ellipsis and surrounding spaces.
 */
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

/**
 * @brief Whether a parent of object is a T.
 */
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
 * @brief Buttons a user can press in a dock or dialog - not including Qt's own (title bars, tab
 * scrollers), not in a toolbar (collected separately) and not committing or deleting.
 */
bool isPressable(const QAbstractButton *button)
{
    return button->isVisible() && !button->objectName().startsWith("qt_")
            && !hasAncestor<QToolBar>(button) && !hasAncestor<QTabBar>(button)
            && !hasAncestor<QDialogButtonBox>(button)
            && !deniedButtons.match(buttonText(button)).hasMatch();
}

/**
 * @brief The menu a tool or push button opens, if any.
 */
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
 * @brief The checked button among those exclusive with @p button, if any.
 */
QAbstractButton *checkedPeer(const QAbstractButton *button)
{
    if (QButtonGroup *group = button->group()) {
        return group->exclusive() ? group->checkedButton() : nullptr;
    }
    if (!button->autoExclusive() || !button->parentWidget()) {
        return nullptr;
    }
    for (auto *peer :
         button->parentWidget()->findChildren<QAbstractButton *>(Qt::FindDirectChildrenOnly)) {
        if (peer->autoExclusive() && peer->isChecked()) {
            return peer;
        }
    }
    return nullptr;
}

/**
 * @brief Clicks the given button, then undoes any check-state change, so later rows start from
 * the same state.
 *
 * For an exclusive group (radio buttons) the button checked before is clicked again. A group that
 * had nothing checked is emptied again in code, as no click can do that.
 */
void clickButton(QAbstractButton *button)
{
    const QPointer<QAbstractButton> guard(button);
    const QPointer<QAbstractButton> previous(checkedPeer(button));
    const bool hadPrevious = previous != nullptr;
    const bool wasChecked = button->isChecked();
    QTest::mouseClick(button, Qt::LeftButton);
    settle();
    if (!guard || !guard->isCheckable() || guard->isChecked() == wasChecked) {
        return;
    }

    QButtonGroup *group = guard->group();
    const bool exclusive = group ? group->exclusive() : guard->autoExclusive();
    if (exclusive && !hadPrevious) {
        // clicks can't empty an exclusive group, lift exclusivity temporarily to uncheck
        if (group) {
            group->setExclusive(false);
            guard->setChecked(false);
            group->setExclusive(true);
        } else {
            guard->setAutoExclusive(false);
            guard->setChecked(false);
            guard->setAutoExclusive(true);
        }
        settle();
        return;
    }

    QAbstractButton *restore = exclusive ? previous.data() : guard.data();
    if (restore && restore->isEnabled() && restore->isVisible()) {
        QTest::mouseClick(restore, Qt::LeftButton);
        settle();
    }
}

/**
 * @brief Visits everything in a window a user can look at without committing, every tab, the first
 * rows of every list, and every pressable button. What those open is handled by the @class Driver.
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

/**
 * @brief The visible entries of a menu, keyed by text.
 *
 * Repeated texts get " #2", " #3", ... appended
 */
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
        items.append(QPair { key, action });
    }
    return items;
}

/**
 * @brief The entry's text with the parts that change removed, so the same entry still matches:
 *
 * e.g:
 * 1. "Edit function \"entry0\" (sym.main)" -> "Edit function \"\""
 * 2. "Add flag at 0x138" -> "Add flag at 0x"
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

/**
 * @brief Closes every open popup, innermost first.
 */
void closePopups()
{
    while (QWidget *popup = QApplication::activePopupWidget()) {
        popup->close();
    }
}

/**
 * @brief Opens the submenu of action by hovering it, as a user would.
 */
bool openSubmenu(QMenu *menu, QAction *action)
{
    menu->setActiveAction(action);
    QMenu *submenu = action->menu();
    return QTest::qWaitFor([submenu]() { return submenu && submenu->isVisible(); },
                           menuTimeoutMs);
}

/**
 * @brief Returns the dock that owns the given object.
 */
CutterDockWidget *owningDock(QObject *object)
{
    for (; object; object = object->parent()) {
        if (auto *dock = qobject_cast<CutterDockWidget *>(object)) {
            return dock;
        }
    }
    return nullptr;
}

/**
 *  Where a user would right-click or double-click: the first row, or the middle.
 */
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

/**
 * @brief Closes popups and every visible top-level window except main and docks.
 */
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
 * @brief Handles every window that opens, except the menus the test opens itself.
 *
 * The first window of each class is explored @ref exploreWindow(), then cancelled, later
 * ones and Qt's own dialogs are cancelled straight away. Both run from a zero timer, so a
 * dialog started with exec() is in its event loop and exec() returns as if the user pressed
 * Cancel. Progress dialogs are left alone, they close when their task ends, and rejecting one
 * cancels the task.
 *
 */
class Driver : public QObject
{
public:
    using MenuCallback = std::function<void(QMenu *)>;

    explicit Driver(QWidget *keep) : keep(keep) { qApp->installEventFilter(this); }
    ~Driver() override { qApp->removeEventFilter(this); }

    /**
     * @brief Hands the next menu that opens to a callback and clicks every action inside
     * that menu. Submenus opened while the callback navigates are left open (meaning no actions
     * inside submenus are clicked).
     *
     * @see menuWasHandled()
     */
    void expectMenu(MenuCallback callback)
    {
        onMenu = std::move(callback);
        menuHandled = false;
        navigating = false;
    }

    /**
     * @brief Returns whether a menu was handled or not.
     *
     * Set to true when a callback has returned
     */
    bool menuWasHandled() const { return menuHandled; }

    /**
     * @brief Call before clicking the entry. From here on, what opens is explored or cancelled
     * instead of let through.
     */
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

/**
 * @brief One thing a user can do. Menu kinds open a menu, then click the entry at path.
 */
struct Step
{
    enum Kind { MenuBar, ToolButton, ToolMenu, ContextMenu, Shortcut, Button, Combo, DoubleClick };

    Kind kind;

    QString name; // shown in the test row, e.g. "Dock/Strings/view"
    QPointer<QWidget> widget; // what gets clicked, focused or right-clicked
    QPointer<QAction> action; // the action behind it, if any (menu title, toolbar button, shortcut)
    QStringList path; // labels to follow to the entry, e.g. {"Edit", "Rename"} (menu kinds only)

    // For steps inside a dock: the dock's name and the widget's key in it, to find them again
    // if an earlier row (resetting the layout) recreated the dock.
    QString dock;
    QString key;

    QPointer<QShortcut> shortcut; // the QShortcut behind a Shortcut step that has no action
};

using Keyed = QList<QPair<QString, QWidget *>>;

/**
 * @brief Adds " #2", " #3", ... to repeated keys.
 */
void addKeyed(Keyed &list, QHash<QString, int> &seen, QString key, QWidget *widget)
{
    const int count = ++seen[key];
    if (count > 1) {
        key += QString(" #%1").arg(count);
    }
    list.append({ key, widget });
}

/**
 * @brief The dock's visible views by class name, as viewports. Empty lists have nothing to click.
 */
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

/**
 * @brief Returns dock's own buttons (not its toolbars), by text.
 */
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

/**
 * @brief The visible combo boxes of the dock with more than one value, outside toolbars.
 */
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

/**
 * @brief The dock's actions that are in no menu or toolbar but have a shortcut, by key.
 */
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

/**
 * @brief Returns the dock's QShortcuts that have a key set, each named as
 * "shortcut Ctrl+F QLineEdit"
 */
QList<QPair<QString, QShortcut *>> dockKeyShortcuts(CutterDockWidget *dock)
{
    QList<QPair<QString, QShortcut *>> shortcuts;
    QHash<QString, int> seen;
    for (QShortcut *shortcut : dock->findChildren<QShortcut *>()) {
        const QWidget *parent = qobject_cast<QWidget *>(shortcut->parent());
        if (!parent || shortcut->key().isEmpty()) {
            continue;
        }
        QString key = "shortcut " + shortcut->key().toString() + " "
                + QString::fromUtf8(parent->metaObject()->className());
        const int count = ++seen[key];
        if (count > 1) {
            key += QString(" #%1").arg(count);
        }
        shortcuts.append({ key, shortcut });
    }
    return shortcuts;
}

/**
 * @brief Selects the entry and emits the signals that would emitted if a user selected it
 */
void chooseComboEntry(QComboBox *combo, int index)
{
    combo->setCurrentIndex(index);
    emit combo->activated(index);
    emit combo->textActivated(combo->itemText(index));
}

bool isComboEntryEnabled(const QComboBox *combo, int index)
{
    const QModelIndex item =
            combo->model()->index(index, combo->modelColumn(), combo->rootModelIndex());
    return combo->model()->flags(item) & Qt::ItemIsEnabled;
}

/**
 * @brief What happened when a step ran. Anything but Clicked skips the row.
 */
enum class Outcome { NoMenu, Missing, Disabled, Clicked };

struct StepResult
{
    Outcome outcome = Outcome::NoMenu;

    // A checkable action (lock panels, tabs on top) is triggered again afterwards, so its state
    // doesn't carry over to later rows.
    QPointer<QAction> toggled;
    bool wasChecked = false;
};

/**
 * @brief Why a step with this outcome was skipped; empty if it ran.
 */
QString skipReason(Outcome outcome, Step::Kind kind)
{
    switch (outcome) {
    case Outcome::NoMenu:
        return kind == Step::ContextMenu ? "the view has no context menu" : "the menu didn't open";
    case Outcome::Missing:
        return "the entry isn't in the menu in this state";
    case Outcome::Disabled:
        return "disabled in this state";
    case Outcome::Clicked:
        break;
    }
    return {};
}

/**
 * @brief Clicks a dock button or a toolbar button, if it's enabled.
 */
void runButtonStep(const Step &step, StepResult &result)
{
    const bool enabled = step.action ? step.action->isEnabled() : step.widget->isEnabled();
    if (!enabled) {
        result.outcome = Outcome::Disabled;
        return;
    }
    result.outcome = Outcome::Clicked;
    if (step.kind == Step::Button) {
        clickButton(static_cast<QAbstractButton *>(step.widget.data()));
        return;
    }
    result.toggled = step.action;
    result.wasChecked = step.action && step.action->isChecked();
    QTest::mouseClick(step.widget.data(), Qt::LeftButton);
}

/**
 * @brief Picks each enabled value of the combo box, then the original one again.
 */
void runComboStep(QComboBox *combo, StepResult &result)
{
    if (!combo->isEnabled()) {
        result.outcome = Outcome::Disabled;
        return;
    }

    const QPointer<QComboBox> guard(combo);
    const int original = combo->currentIndex();
    const int values = std::min(combo->count(), maxComboValues);
    for (int i = 0; guard && i < values; ++i) {
        if (i != original && isComboEntryEnabled(guard, i)) {
            chooseComboEntry(guard, i);
            QVERIFY(settle());
        }
    }
    if (guard && original >= 0 && guard->currentIndex() != original) {
        chooseComboEntry(guard, original);
        QVERIFY(settle());
    }
    result.outcome = Outcome::Clicked;
}

void runDoubleClickStep(QWidget *viewport, StepResult &result)
{
    result.outcome = Outcome::Clicked;
    QTest::mouseDClick(viewport, Qt::LeftButton, {}, clickPoint(viewport));
}

/**
 * @brief Triggers the checkable action the step toggled again, if its state changed.
 */
void restoreToggled(const StepResult &result)
{
    QAction *action = result.toggled;
    if (action && action->isCheckable() && action->isChecked() != result.wasChecked) {
        action->trigger();
        QVERIFY(settle());
    }
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
    /**
     * @brief Test whether docks open and their views show without crashing.
     */
    void everyDockOpens();

    void everyEntryRuns_data();
    /**
     * @brief Test whether entries runs without crashing or hanging
     *
     * The entries contain:
     * 1. Every menu bar entry (submenus opened by hovering)
     * 2. Toolbar button
     * 3. Dropdown entry
     * 4. Dock button
     * 5. Combo box value
     * 6. List double-click
     * 7. Shortcut that isn't in a menu
     *
     * The first window of each class that opens is explored (tabs, list rows, buttons
     * that don't apply or close anything) and then closed with reject(), as Cancel would.
     *
     * Entries that leave the test or change the fixture are in deniedActions; buttons that
     * apply, delete or close (OK, Save, Delete, ...) match deniedButtons.
     */
    void everyEntryRuns();

    /**
     * @brief The context menus are read while collecting the entries, which normally runs first.
     */
    void everyContextEntryRuns_data();
    /**
     * @brief Test whether dock's context menu entries run without crashing or hanging.
     */
    void everyContextEntryRuns();

    void everyDockOpensWhileEmulating_data();
    /**
     * @brief The dock opens under emulation.
     */
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
    void runMenuStep(const Step &step, StepResult &result);
    void runShortcutStep(const Step &step, StepResult &result);
    QSet<QString> dockNames() const;
    void closeDocksNotIn(const QSet<QString> &names);
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

    // use main. It has calls, jumps and strings, so more context-menu entries are enabled compared
    // to a stub
    target = CutterTest::core()->math("main");
    QVERIFY2(target != 0, "the fixture has no main");
    driver = std::make_unique<Driver>(main);
}

void TestUiSweep::init()
{
    closeOtherWindows(main);
    driver->reset();

    // drop windows recorded before, so "opened" lists only the windows that this row opened
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
 * The dock may already be the shown one but hidden behind another tab, in that case it is raised.
 */
void TestUiSweep::showDock(CutterDockWidget *dock)
{
    // memory docks delete themselves when closed, so leave open
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

/**
 * @brief Lists the menu's entries. Open holds the menus above it, so a menu inside itself ends.
 * @param menu The menu to list.
 * @param prefix The prefix for path.
 * @param paths Full list of labels to reach an entry inside menu.
 * @param open Holds the menus currently being walked, so a submenu that leads back to one of them
 * is recorded as an entry instead of being walked forever.
 */
void TestUiSweep::listMenu(QMenu *menu, const QStringList &prefix, QList<QStringList> &paths,
                           QSet<QMenu *> &open)
{
    open << menu;
    for (const auto &[key, action] : menuItems(menu)) {
        // use only the first entry for repeated items.
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

/**
 * @brief Opens the menu the way a user would. Returns false if no menu opened.
 */
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
 * @brief Opens the menu, reads its entries and appends a step per entry.
 * @param opener The step that opens the menu, each new step copies it and sets path to an entry.
 * @param steps The list the new steps are appended to.
 * @param dedupe If true, add nothing when an earlier dock of the same class already gave a menu
 * with the same entries.
 */
void TestUiSweep::addMenuSteps(const Step &opener, QList<Step> &steps, bool dedupe)
{
    // collection runs before any row, so an error here would name nothing unless explicitly printed
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
 * @brief Finds everything a user can do via GUI. Opening context menus, clicking toolbar buttons
 * etc. Context-menu entries go to contextSteps, the rest to entrySteps.
 *
 * All of these steps are found at runtime so any additions to src/ are tested automatically,
 * provided the current implementation can reach it.
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

            // onlyMenu steps are already covered above
            if (!onlyMenu) {
                entrySteps << Step { Step::Button, prefix + key, button, {}, {}, dockName, key };
            }
        }
        for (const auto &[key, widget] : dockCombos(dock)) {
            entrySteps << Step { Step::Combo, prefix + key, widget, {}, {}, dockName, key };
        }

        // shortcuts that are not in any menu or toolbar, only the key reaches them.
        for (const auto &[key, action] : dockShortcuts(dock)) {
            auto *widget = qobject_cast<QWidget *>(action->associatedObjects().first());
            entrySteps << Step { Step::Shortcut, prefix + key, widget, action, {},
                                 dockName,       key };
        }
        // QShortcuts have no action at all, so only the key reaches them too.
        for (const auto &[key, shortcut] : dockKeyShortcuts(dock)) {
            auto *widget = qobject_cast<QWidget *>(shortcut->parent());
            entrySteps << Step { Step::Shortcut, prefix + key, widget, {}, {},
                                 dockName,       key,          shortcut };
        }
    }
    showDock(nullptr);
}

/**
 * @brief Reconnects a step to its widget after the dock it pointed to was deleted and rebuilt.
 */
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
        for (const auto &[key, shortcut] : dockKeyShortcuts(dock)) {
            if (withoutState(key) == withoutState(step.key)) {
                step.shortcut = shortcut;
                step.widget = qobject_cast<QWidget *>(shortcut->parent());
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
 * @brief Runs a step, then puts back what it changed so later rows start from the same state.
 *
 * Sets skipped to why the step couldn't run; leaves it empty if it ran. An out-parameter, as
 * QVERIFY needs a void function.
 */
void TestUiSweep::runStep(const Step &original, QString &skipped)
{
    Step step = original;
    if (!step.widget && !step.dock.isEmpty()) {
        findAgain(step);
    }
    if (!step.widget) {
        skipped = "the widget no longer exists";
        return;
    }
    showDock(owningDock(step.widget.data()));
    if (QTest::currentTestFailed()) {
        return;
    }

    // take a snapshot so we can close the docks that this step opens
    const QSet<QString> docksBefore = dockNames();
    StepResult result;
    {
        const CutterTest::DeadlockWatchdog watchdog(QTest::currentDataTag(), hangTimeout);
        // A crash kills the process before the row is reported, so print the name here
        qInfo().noquote() << "running" << QTest::currentDataTag();

        switch (step.kind) {
        case Step::MenuBar:
        case Step::ToolMenu:
        case Step::ContextMenu:
            runMenuStep(step, result);
            break;
        case Step::ToolButton:
        case Step::Button:
            runButtonStep(step, result);
            break;
        case Step::Combo:
            runComboStep(static_cast<QComboBox *>(step.widget.data()), result);
            break;
        case Step::Shortcut:
            runShortcutStep(step, result);
            break;
        case Step::DoubleClick:
            runDoubleClickStep(step.widget.data(), result);
            break;
        }
        if (QTest::currentTestFailed()) {
            return;
        }
        QVERIFY(settle());
    }

    restoreToggled(result);
    if (QTest::currentTestFailed()) {
        return;
    }
    const QStringList opened = driver->takeSeen();
    if (!opened.isEmpty()) {
        qInfo() << "opened" << opened;
    }
    closeDocksNotIn(docksBefore);
    closeOtherWindows(main);
    skipped = skipReason(result.outcome, step.kind);
}

/**
 * @brief Opens the step's menu and clicks the entry at its path, if it's there and enabled.
 */
void TestUiSweep::runMenuStep(const Step &step, StepResult &result)
{
    driver->expectMenu([this, &step, &result](QMenu *menu) {
        for (int i = 0; i < step.path.size(); ++i) {
            QAction *action = findEntry(menu, step.path[i]);
            if (!action) {
                result.outcome = Outcome::Missing;
                break;
            }
            if (!action->isEnabled()) {
                result.outcome = Outcome::Disabled;
                break;
            }
            if (i + 1 < step.path.size()) {
                if (!openSubmenu(menu, action)) {
                    result.outcome = Outcome::Missing;
                    break;
                }
                menu = action->menu();
                continue;
            }
            result.outcome = Outcome::Clicked;
            result.toggled = action;
            result.wasChecked = action->isChecked();
            driver->stopNavigating();
            QTest::mouseClick(menu, Qt::LeftButton, {}, menu->actionGeometry(action).center());
            return;
        }
        closePopups();
    });
    openMenu(step);
    driver->reset();
}

/**
 * @brief Presses the step's key on its widget, if the action or QShortcut behind it is enabled.
 */
void TestUiSweep::runShortcutStep(const Step &step, StepResult &result)
{
    QKeySequence keys;
    if (step.shortcut) {
        if (!step.shortcut->isEnabled()) {
            result.outcome = Outcome::Disabled;
            return;
        }
        keys = step.shortcut->key();
    } else {
        if (!step.action || !step.action->isEnabled()) {
            result.outcome = Outcome::Disabled;
            return;
        }
        result.toggled = step.action;
        result.wasChecked = step.action->isChecked();
        keys = step.action->shortcut();
    }
    result.outcome = Outcome::Clicked;
    main->activateWindow();
    step.widget->setFocus(Qt::ShortcutFocusReason);
    QTest::keySequence(step.widget.data(), keys);
}

/**
 * @brief Names of the docks open now.
 */
QSet<QString> TestUiSweep::dockNames() const
{
    QSet<QString> names;
    for (const CutterDockWidget *dock : main->findChildren<CutterDockWidget *>()) {
        names << dock->objectName();
    }
    return names;
}

/**
 * @brief Closes the docks whose name isn't in @p names.
 */
void TestUiSweep::closeDocksNotIn(const QSet<QString> &names)
{
    for (CutterDockWidget *dock : main->findChildren<CutterDockWidget *>()) {
        if (!names.contains(dock->objectName())) {
            dock->close();
        }
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
