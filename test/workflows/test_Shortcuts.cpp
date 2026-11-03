/**
 * @file test_Shortcuts.cpp
 * @brief Keyboard shortcuts reach the widget that has focus, and only that one.
 *
 * Many docks register the same key (Copy, Find) with a widget-local context. A wrong
 * context makes the key ambiguous so nothing happens, or lets another dock handle it.
 * The hexdump copy shortcut once did nothing at all.
 */

#include "Fixtures.h"
#include "TestEnv.h"
#include "ViewHelpers.h"

#include "shortcuts/ShortcutManager.h"
#include "widgets/AbstractFilterView.h"
#include "widgets/FunctionsWidget.h"
#include "widgets/HexdumpWidget.h"
#include "widgets/StringsWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QScopeGuard>
#include <QTest>
#include <QTreeView>

namespace {

constexpr int copiedBytes = 4;

bool focusIsWithin(QWidget *widget)
{
    QWidget *focus = QApplication::focusWidget();
    return focus && (focus == widget || widget->isAncestorOf(focus));
}

} // namespace

class TestShortcuts : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void hexdumpCopyShortcutCopiesSelection();
    void filterShortcutOpensOnlyFocusedListFilter();

private:
    MainWindow *main = nullptr;
};

void TestShortcuts::initTestCase()
{
    main = CutterTest::mainWindow();
    QVERIFY(main);
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

void TestShortcuts::filterShortcutOpensOnlyFocusedListFilter()
{
    auto *functions = main->findChild<FunctionsWidget *>();
    auto *strings = main->findChild<StringsWidget *>();
    QVERIFY(functions);
    QVERIFY(strings);
    strings->toggleDockWidget(true);
    strings->raiseMemoryWidget();
    functions->raiseMemoryWidget();

    auto *functionsFilter = functions->findChild<AbstractFilterView *>();
    auto *stringsFilter = strings->findChild<AbstractFilterView *>();
    auto *functionsList = functions->findChild<QTreeView *>();
    QVERIFY(functionsFilter);
    QVERIFY(stringsFilter);
    QVERIFY(functionsList);

    const QList<QKeySequence> keys = Shortcuts()->getKeySequences("General.showFilter");
    QVERIFY(!keys.isEmpty());

    functionsFilter->closeFilter();
    const auto restore = qScopeGuard([functionsFilter]() { functionsFilter->closeFilter(); });
    const bool stringsFilterVisible = stringsFilter->isVisible();

    functionsList->setFocus(Qt::OtherFocusReason);
    QTRY_VERIFY(functionsList->hasFocus());

    QTest::keySequence(functionsList, keys.first());

    QTRY_VERIFY(functionsFilter->isVisible());
    QTRY_VERIFY(QApplication::focusWidget() == functionsFilter->lineEdit());
    QCOMPARE(stringsFilter->isVisible(), stringsFilterVisible);
}

CUTTER_APP_TEST_MAIN_WITH_FILE(TestShortcuts, CutterTest::Fixtures::helloX64())
#include "test_Shortcuts.moc"
