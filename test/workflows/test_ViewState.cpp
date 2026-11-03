/**
 * @file test_ViewState.cpp
 * @brief Editing in a code view keeps the user's place in it.
 *
 * An edit refreshes the code views. If the refresh resets scroll, cursor or viewport,
 * the user loses their place after every rename or comment; renaming a variable in the
 * decompiler used to jump back to the top. Each test moves away from the default
 * position, makes an edit that refreshes the view, and checks the view stayed put.
 */

#include "Fixtures.h"
#include "TestEnv.h"
#include "ViewHelpers.h"

#include "widgets/DecompilerWidget.h"

#include <QPlainTextEdit>
#include <QScopeGuard>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTest>

namespace {

constexpr int viewRefreshTimeoutMs = 10000;
constexpr int decompileTimeoutMs = 60000;
constexpr int settleMs = 500;
constexpr int scrolledInstructions = 3;
constexpr int graphScrollPixels = 40;
constexpr RVA minimumBlocks = 4;

} // namespace

class TestViewState : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void disassemblyKeepsTopLineAndSeekAfterComment();
    void graphKeepsViewportAfterComment();
    void decompilerKeepsScrollAfterVariableRename();

private:
    MainWindow *main = nullptr;
    DisassemblyWidget *disassembly = nullptr;
    GraphWidget *graph = nullptr;
    RVA target = RVA_INVALID;
};

void TestViewState::initTestCase()
{
    main = CutterTest::mainWindow();
    QVERIFY(main);
    disassembly = CutterTest::findSynchronizedDock<DisassemblyWidget>(main);
    graph = CutterTest::findSynchronizedDock<GraphWidget>(main);
    QVERIFY(disassembly);
    QVERIFY(graph);

    CutterCore *core = CutterTest::core();
    RVA largestSize = 0;
    const QList<FunctionDescription> functions = core->getAllFunctions();
    for (const FunctionDescription &function : functions) {
        if (function.nbbs >= minimumBlocks && function.linearSize > largestSize
            && !core->getVariables(function.offset).isEmpty()) {
            target = function.offset;
            largestSize = function.linearSize;
        }
    }
    QVERIFY2(target != RVA_INVALID, "no analyzed function with variables and several blocks");
}

void TestViewState::init()
{
    CutterTest::core()->seek(target);
}

void TestViewState::disassemblyKeepsTopLineAndSeekAfterComment()
{
    CutterCore *core = CutterTest::core();
    disassembly->raiseMemoryWidget();
    QTRY_VERIFY_WITH_TIMEOUT(CutterTest::disassemblyOffsets(disassembly).contains(target),
                             viewRefreshTimeoutMs);

    const RVA topBefore = CutterTest::disassemblyTopOffset(disassembly);
    disassembly->scrollInstructions(scrolledInstructions);
    const RVA top = CutterTest::disassemblyTopOffset(disassembly);
    QVERIFY(top != RVA_INVALID);
    QVERIFY(top != topBefore);
    const RVA seek = core->getOffset();

    RVA address = RVA_INVALID;
    const QList<DisassemblyLine> lines = disassembly->getLines();
    for (int i = disassembly->getStartIndex() + 1; i < disassembly->getEndIndex(); ++i) {
        if (core->getCommentAt(lines.at(i).offset).isEmpty()) {
            address = lines.at(i).offset;
            break;
        }
    }
    QVERIFY2(address != RVA_INVALID, "no uncommented line in view");

    const QString comment = QStringLiteral("cutter_test_view_state");
    const auto restore = qScopeGuard([&]() { core->delComment(address); });
    core->setComment(address, comment);

    QTRY_VERIFY_WITH_TIMEOUT(CutterTest::disassemblyContains(disassembly, comment),
                             viewRefreshTimeoutMs);
    QCOMPARE(CutterTest::disassemblyTopOffset(disassembly), top);
    QCOMPARE(core->getOffset(), seek);
}

void TestViewState::graphKeepsViewportAfterComment()
{
    CutterCore *core = CutterTest::core();
    graph->raiseMemoryWidget();
    QTRY_VERIFY_WITH_TIMEOUT(CutterTest::graphHasBlock(graph, target), viewRefreshTimeoutMs);
    QTest::qWait(settleMs);

    DisassemblerGraphView *view = graph->getGraphView();
    const QPoint start = view->getViewOffset();
    view->setViewOffset(start + QPoint(0, graphScrollPixels));
    const QPoint scrolled = view->getViewOffset();
    if (scrolled == start) {
        QSKIP("the graph fits in the view, nothing to scroll");
    }

    RVA address = RVA_INVALID;
    const auto blocks = view->getBlocks();
    for (const auto &block : blocks) {
        if (block.first != target && core->getCommentAt(block.first).isEmpty()) {
            address = block.first;
            break;
        }
    }
    QVERIFY2(address != RVA_INVALID, "no uncommented block besides the entry");

    QSignalSpy refreshed(view, &CutterGraphView::viewRefreshed);
    const auto restore = qScopeGuard([&]() { core->delComment(address); });
    core->setComment(address, QStringLiteral("cutter_test_view_state"));

    QTRY_VERIFY_WITH_TIMEOUT(refreshed.count() > 0, viewRefreshTimeoutMs);
    QTest::qWait(settleMs);
    QCOMPARE(view->getViewOffset(), scrolled);
}

void TestViewState::decompilerKeepsScrollAfterVariableRename()
{
    CutterCore *core = CutterTest::core();
    if (core->getDecompilers().isEmpty()) {
        QSKIP("Cutter was built without a decompiler");
    }
    auto *decompiler = CutterTest::findSynchronizedDock<DecompilerWidget>(main);
    QVERIFY(decompiler);
    auto *text = decompiler->findChild<QPlainTextEdit *>("textEdit");
    QVERIFY(text);

    decompiler->raiseMemoryWidget();
    QTRY_VERIFY_WITH_TIMEOUT(!text->toPlainText().isEmpty(), decompileTimeoutMs);
    QTest::qWait(settleMs);

    QString oldName;
    const QList<VariableDescription> variables = core->getVariables(target);
    for (const VariableDescription &variable : variables) {
        if (text->toPlainText().contains(variable.name)) {
            oldName = variable.name;
            break;
        }
    }
    if (oldName.isEmpty()) {
        QSKIP("no variable of the function appears in the decompiled code");
    }

    QScrollBar *scroll = text->verticalScrollBar();
    if (scroll->maximum() == 0) {
        QSKIP("the decompiled function fits in the view, nothing to scroll");
    }
    scroll->setValue(scroll->maximum() / 2);
    const int position = scroll->value();

    const QString newName = QStringLiteral("cutter_test_view_state_var");
    const auto restore =
            qScopeGuard([&]() { core->renameFunctionVariable(oldName, newName, target); });
    core->renameFunctionVariable(newName, oldName, target);

    QTRY_VERIFY_WITH_TIMEOUT(text->toPlainText().contains(newName), decompileTimeoutMs);
    QTest::qWait(settleMs);
    QCOMPARE(scroll->value(), position);
}

CUTTER_APP_TEST_MAIN_WITH_FILE(TestViewState, CutterTest::Fixtures::coreutilsLs())
#include "test_ViewState.moc"
