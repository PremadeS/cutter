/**
 * @file test_Shortcuts.cpp
 * @brief Keyboard shortcuts reach the widget that has focus, and only that one.
 *
 * Many docks register the same key (Copy, Find) with a widget-local context. A wrong
 * context makes the key ambiguous so nothing happens, or lets another dock handle it.
 * The hexdump copy shortcut once did nothing at all, and the disassembly's Copy and Edit
 * function stopped working by key after their context menu was opened where they don't apply. Every filter bar wires Find and
 * Esc itself; the Xrefs dialog once shipped with neither working. Filter bars in docks
 * and in the dialogs opened in initTestCase are found at runtime and all checked.
 *
 * Two shortcuts with the same key clash when some focus widget is inside both of their
 * areas: a window-wide shortcut covers the whole main window, a widget-with-children one
 * its widget and everything in it, a widget one only its widget. Qt then runs neither.
 * Every enabled shortcut and action of the main window is checked that way, after adding
 * a second view of each kind, since several views of a kind once each registered the same
 * window-wide key.
 */

#include "Fixtures.h"
#include "TestEnv.h"
#include "ViewHelpers.h"
#include "dialogs/TypesVariablesDialog.h"
#include "dialogs/XrefsDialog.h"
#include "menus/DisassemblyContextMenu.h"
#include "shortcuts/ShortcutManager.h"
#include "widgets/AbstractFilterView.h"
#include "widgets/CutterDockWidget.h"
#include "widgets/HexdumpWidget.h"
#include "widgets/QuickFilterView.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDockWidget>
#include <QHash>
#include <QMenu>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QPointer>
#include <QScopeGuard>
#include <QShortcut>
#include <QTest>
#include <QTextBlock>

#include <memory>

namespace {

constexpr int copiedBytes = 4;
constexpr int copiedLines = 3;

bool focusIsWithin(QWidget *widget)
{
    QWidget *focus = QApplication::focusWidget();
    return focus && (focus == widget || widget->isAncestorOf(focus));
}

/** The list a filter bar filters: the item view closest to it among its siblings. */
QAbstractItemView *pairedList(QWidget *filter)
{
    for (QWidget *child = filter; child->parentWidget(); child = child->parentWidget()) {
        const QObjectList siblings = child->parentWidget()->children();
        const qsizetype own = siblings.indexOf(child);
        QAbstractItemView *closest = nullptr;
        qsizetype distance = siblings.size();
        for (qsizetype i = 0; i < siblings.size(); ++i) {
            auto *view = qobject_cast<QAbstractItemView *>(siblings.at(i));
            if (view && qAbs(i - own) < distance) {
                closest = view;
                distance = qAbs(i - own);
            }
        }
        if (closest) {
            return closest;
        }
    }
    return nullptr;
}

/** One key of a shortcut or action, and the widget its context is relative to. */
struct Binding
{
    const QObject *owner;
    QString name;
    QKeySequence key;
    Qt::ShortcutContext context;
    QWidget *scope;
};

QString widgetName(const QWidget *widget)
{
    const QString name = widget->objectName();
    QString path = name.isEmpty() ? QString::fromLatin1(widget->metaObject()->className()) : name;
    for (const QWidget *parent = widget->parentWidget(); parent; parent = parent->parentWidget()) {
        if (qobject_cast<const QDockWidget *>(parent)) {
            return parent->objectName() + '/' + path;
        }
    }
    return path;
}

QString contextName(Qt::ShortcutContext context, const QWidget *scope)
{
    switch (context) {
    case Qt::WidgetShortcut:
        return " on " + widgetName(scope);
    case Qt::WidgetWithChildrenShortcut:
        return " in " + widgetName(scope);
    default:
        return " window-wide from " + widgetName(scope);
    }
}

bool reachableFromMenuBar(const QMenu *menu)
{
    const QObjectList owners = menu->menuAction()->associatedObjects();
    for (const QObject *owner : owners) {
        if (qobject_cast<const QMenuBar *>(owner)) {
            return true;
        }
        const auto *parentMenu = qobject_cast<const QMenu *>(owner);
        if (parentMenu && parentMenu != menu && reachableFromMenuBar(parentMenu)) {
            return true;
        }
    }
    return false;
}

void addKeys(QList<Binding> &bindings, const QObject *owner, const QString &name,
             const QList<QKeySequence> &keys, Qt::ShortcutContext context, QWidget *scope)
{
    for (const QKeySequence &key : keys) {
        if (!key.isEmpty()) {
            bindings.append({ owner, name, key, context, scope });
        }
    }
}

QList<Binding> windowBindings(QWidget *window)
{
    QList<Binding> bindings;
    const QList<QShortcut *> shortcuts = window->findChildren<QShortcut *>();
    for (const QShortcut *shortcut : shortcuts) {
        auto *scope = qobject_cast<QWidget *>(shortcut->parent());
        if (shortcut->isEnabled() && scope && scope->window() == window) {
            addKeys(bindings, shortcut, "shortcut" + contextName(shortcut->context(), scope),
                    shortcut->keys(), shortcut->context(), scope);
        }
    }

    const QList<QAction *> actions = window->findChildren<QAction *>();
    for (const QAction *action : actions) {
        if (!action->isEnabled()) {
            continue;
        }
        const QString name = "\"" + action->text().remove('&') + "\"";
        const QObjectList owners = action->associatedObjects();
        for (QObject *owner : owners) {
            auto *scope = qobject_cast<QWidget *>(owner);
            if (const auto *menu = qobject_cast<const QMenu *>(owner)) {
                scope = reachableFromMenuBar(menu) ? window : nullptr;
            }
            if (scope && scope->window() == window) {
                addKeys(bindings, action, name + contextName(action->shortcutContext(), scope),
                        action->shortcuts(), action->shortcutContext(), scope);
            }
        }
    }
    return bindings;
}

bool isWindowWide(const Binding &binding)
{
    return binding.context == Qt::WindowShortcut || binding.context == Qt::ApplicationShortcut;
}

bool reaches(const Binding &binding, const QWidget *focus)
{
    return focus == binding.scope
            || (binding.context == Qt::WidgetWithChildrenShortcut
                && binding.scope->isAncestorOf(focus));
}

bool canBeActiveTogether(const Binding &a, const Binding &b)
{
    return isWindowWide(a) || isWindowWide(b) || reaches(a, b.scope) || reaches(b, a.scope);
}

} // namespace

class TestShortcuts : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void hexdumpCopyShortcutCopiesSelection();
    void disassemblyShortcutsWorkAfterContextMenu();
    /**
     * @brief Every filter bar: Find shows and focuses it, Esc clears it, Esc again closes it
     * (if it can close), and Find brings it back. Other filter bars stay as they were.
     */
    void filterBarKeyboardCycle_data();
    void filterBarKeyboardCycle();
    void noTwoShortcutsShareAKeyWhereBothWork();

private:
    QString filterRowName(AbstractFilterView *filter) const;

    MainWindow *main = nullptr;
    std::unique_ptr<XrefsDialog> xrefs;
    std::unique_ptr<TypesVariablesDialog> typeVariables;
    QMap<QString, QPointer<AbstractFilterView>> filters;
};

void TestShortcuts::initTestCase()
{
    main = CutterTest::mainWindow();
    QVERIFY(main);
    main->activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(main));

    xrefs = std::make_unique<XrefsDialog>(main);
    xrefs->fillRefsForAddress(CutterTest::core()->getOffset(), "cutter_test", true);
    typeVariables = std::make_unique<TypesVariablesDialog>(main, "int");

    const QList<AbstractFilterView *> bars = main->findChildren<AbstractFilterView *>();
    for (AbstractFilterView *bar : bars) {
        filters.insert(filterRowName(bar), bar);
    }
}

void TestShortcuts::cleanup()
{
    const QWidgetList windows = QApplication::topLevelWidgets();
    for (QWidget *window : windows) {
        if (window != main && window->isVisible()) {
            window->hide();
        }
    }
    main->activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(main));
}

void TestShortcuts::hexdumpCopyShortcutCopiesSelection()
{
    auto *hexdump = CutterTest::findSynchronizedDock<HexdumpWidget>(main);
    QVERIFY(hexdump);
    hexdump->raiseMemoryWidget();
    auto *hex = hexdump->findChild<HexWidget *>();
    QVERIFY(hex);

    const QList<QKeySequence> keys = Shortcuts()->getKeySequences("Hex.copy");
    QVERIFY(!keys.isEmpty());

    const RVA start = CutterTest::core()->getOffset();
    hex->selectRange(start, start + copiedBytes - 1);
    const QString expected =
            QString::fromLatin1(CutterTest::core()->ioRead(start, copiedBytes).toHex());

    QApplication::clipboard()->clear();
    hex->setFocus(Qt::OtherFocusReason);
    QTRY_VERIFY(focusIsWithin(hex));

    QTest::keySequence(QApplication::focusWidget(), keys.first());
    QTRY_COMPARE(QApplication::clipboard()->text(), expected);
}

void TestShortcuts::disassemblyShortcutsWorkAfterContextMenu()
{
    CutterCore *core = CutterTest::core();
    auto *disassembly = CutterTest::findSynchronizedDock<DisassemblyWidget>(main);
    QVERIFY(disassembly);
    disassembly->raiseMemoryWidget();
    auto *menu = disassembly->findChild<DisassemblyContextMenu *>();
    QVERIFY(menu);
    auto *edit = qobject_cast<QPlainTextEdit *>(disassembly->getTextWidget());
    QVERIFY(edit);
    QTRY_VERIFY(disassembly->getEndIndex() - disassembly->getStartIndex() >= copiedLines);

    // Open and close the menu where Copy and Edit function don't apply: nothing selected, and
    // an address outside any function
    RVA outsideFunctions = RVA_INVALID;
    const QList<SectionDescription> sections = core->getAllSections();
    for (const SectionDescription &section : sections) {
        if (section.vaddr && section.vsize && !core->functionIn(section.vaddr)) {
            outsideFunctions = section.vaddr;
            break;
        }
    }
    QVERIFY2(outsideFunctions != RVA_INVALID, "no section outside the functions");
    const auto restoreOffset = qScopeGuard([&]() { menu->setOffset(core->getOffset()); });
    menu->setOffset(outsideFunctions);
    menu->setCanCopy(false);
    menu->popup(disassembly->mapToGlobal(QPoint()));
    QTRY_VERIFY(menu->isVisible());
    menu->hide();

    const QList<QAction *> actions = menu->findChildren<QAction *>();
    for (QAction *action : actions) {
        if (!action->shortcut().isEmpty()) {
            QVERIFY2(action->isVisible(),
                     qPrintable(action->text() + " stays hidden, so its shortcut does nothing"));
        }
    }

    // Select the first lines and copy them by key
    disassembly->updateSelectionAnchor(QTextCursor(edit->document()));
    QTextCursor end(edit->document()->findBlockByNumber(copiedLines - 1));
    end.movePosition(QTextCursor::EndOfBlock);
    disassembly->updateSelectionPos(end);
    disassembly->refreshDisasm(RVA_INVALID, RefreshMode::None);

    const QList<QKeySequence> keys = Shortcuts()->getKeySequences("Disassembly.copy");
    QVERIFY(!keys.isEmpty());
    QApplication::clipboard()->clear();
    disassembly->setFocus(Qt::OtherFocusReason);
    QTRY_VERIFY(focusIsWithin(disassembly));

    QTest::keySequence(QApplication::focusWidget(), keys.first());
    QTRY_COMPARE(QApplication::clipboard()->text().split('\n').size(), copiedLines);
}

QString TestShortcuts::filterRowName(AbstractFilterView *filter) const
{
    QString owner;
    if (filter->window() == main) {
        for (QWidget *parent = filter->parentWidget(); parent; parent = parent->parentWidget()) {
            if (qobject_cast<CutterDockWidget *>(parent)) {
                owner = parent->objectName();
                break;
            }
        }
    } else {
        owner = filter->window()->metaObject()->className();
    }
    return owner + '/' + filter->objectName();
}

void TestShortcuts::filterBarKeyboardCycle_data()
{
    QTest::addColumn<QString>("filter");
    for (auto it = filters.cbegin(); it != filters.cend(); ++it) {
        QTest::newRow(qPrintable(it.key())) << it.key();
    }
}

void TestShortcuts::filterBarKeyboardCycle()
{
    QFETCH(const QString, filter);
    AbstractFilterView *bar = filters.value(filter);
    QVERIFY(bar);

    QWidget *window = bar->window();
    if (window == main) {
        auto *dock = bar->parentWidget();
        while (dock && !qobject_cast<CutterDockWidget *>(dock)) {
            dock = dock->parentWidget();
        }
        QVERIFY(dock);
        static_cast<CutterDockWidget *>(dock)->toggleDockWidget(true);
        static_cast<CutterDockWidget *>(dock)->raiseMemoryWidget();
    } else {
        window->show();
    }
    const auto hideDialog = qScopeGuard([window, this]() {
        if (window != main) {
            window->hide();
        }
    });
    window->activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(window));

    QAbstractItemView *list = pairedList(bar);
    QVERIFY2(list, "no list next to the filter bar");
    const bool closable = qobject_cast<QuickFilterView *>(bar) != nullptr;
    if (closable) {
        bar->closeFilter();
    } else {
        bar->clearFilter();
    }

    QHash<AbstractFilterView *, bool> othersVisible;
    for (AbstractFilterView *other : std::as_const(filters)) {
        if (other && other != bar) {
            othersVisible.insert(other, other->isVisible());
        }
    }

    const QKeySequence showKey = Shortcuts()->getKeySequences("General.showFilter").value(0);
    const QKeySequence clearKey = Shortcuts()->getKeySequences("General.clearFilter").value(0);
    QVERIFY(!showKey.isEmpty() && !clearKey.isEmpty());

    list->setFocus(Qt::OtherFocusReason);
    QTRY_VERIFY(list->hasFocus());
    QTest::keySequence(list, showKey);
    QTRY_VERIFY2(bar->isVisible() && QApplication::focusWidget() == bar->lineEdit(),
                 "the find shortcut did not show and focus the filter");
    for (auto it = othersVisible.cbegin(); it != othersVisible.cend(); ++it) {
        QVERIFY2(it.key()->isVisible() == it.value(), "the find shortcut changed another filter");
    }

    QTest::keyClicks(bar->lineEdit(), "cutter");
    QTest::keySequence(bar->lineEdit(), clearKey);
    QTRY_VERIFY2(bar->lineEdit()->text().isEmpty(), "Esc did not clear the filter");
    QVERIFY2(window->isVisible(), "Esc closed the window instead of clearing the filter");

    if (!closable) {
        return;
    }
    QTest::keySequence(bar->lineEdit(), clearKey);
    QTRY_VERIFY2(!bar->isVisible(), "Esc on an empty filter did not close it");
    QVERIFY2(window->isVisible(), "Esc closed the window instead of the filter");

    list->setFocus(Qt::OtherFocusReason);
    QTRY_VERIFY(list->hasFocus());
    QTest::keySequence(list, showKey);
    QTRY_VERIFY2(bar->isVisible() && QApplication::focusWidget() == bar->lineEdit(),
                 "the find shortcut did not bring a closed filter back");
}

void TestShortcuts::noTwoShortcutsShareAKeyWhereBothWork()
{
    const char *const addViewActions[] = { "actionExtraDisassembly", "actionExtraGraph",
                                           "actionExtraHexdump", "actionExtraDecompiler" };
    for (const char *actionName : addViewActions) {
        auto *addView = main->findChild<QAction *>(actionName);
        QVERIFY2(addView, actionName);
        addView->trigger();
    }
    QCoreApplication::processEvents();

    const QList<Binding> bindings = windowBindings(main);
    QStringList clashes;
    for (qsizetype i = 0; i < bindings.size(); ++i) {
        for (qsizetype j = i + 1; j < bindings.size(); ++j) {
            const Binding &a = bindings.at(i);
            const Binding &b = bindings.at(j);
            if (a.owner != b.owner && a.key == b.key && canBeActiveTogether(a, b)) {
                clashes << QString("%1 and %2 share %3")
                                   .arg(a.name, b.name,
                                        a.key.toString(QKeySequence::PortableText));
            }
        }
    }

    clashes.removeDuplicates();
    clashes.sort();
    QVERIFY2(clashes.isEmpty(), qPrintable(clashes.join("\n")));
}

CUTTER_APP_TEST_MAIN_WITH_FILE(TestShortcuts, CutterTest::Fixtures::helloX64())
#include "test_Shortcuts.moc"
