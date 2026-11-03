/**
 * @file test_EditPropagation.cpp
 * @brief An edit made anywhere reaches every view that shows the edited thing.
 *
 * Each view refreshes from its own set of core signals. A view that misses one keeps
 * showing the old function name, comment or variable until something else forces a
 * refresh, which users see as edits that "didn't apply" in some views.
 */

#include "Fixtures.h"
#include "TestEnv.h"
#include "ViewHelpers.h"

#include "widgets/CommentsWidget.h"
#include "widgets/DecompilerWidget.h"
#include "widgets/FunctionsWidget.h"

#include <QPlainTextEdit>
#include <QScopeGuard>
#include <QTest>
#include <QTreeView>

namespace {

constexpr int viewRefreshTimeoutMs = 10000;
constexpr int decompileTimeoutMs = 60000;

QString functionName(CutterCore *core, RVA address)
{
    const QList<FunctionDescription> functions = core->getAllFunctions();
    for (const FunctionDescription &function : functions) {
        if (function.offset == address) {
            return function.name;
        }
    }
    return {};
}

} // namespace

class TestEditPropagation : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void renamedFunctionShowsInEveryView();
    void commentShowsInEveryView();
    void renamedVariableShowsInDisassembly();
    void decompilerShowsEdits();

private:
    MainWindow *main = nullptr;
    FunctionsWidget *functions = nullptr;
    CommentsWidget *comments = nullptr;
    DisassemblyWidget *disassembly = nullptr;
    GraphWidget *graph = nullptr;
    RVA target = RVA_INVALID;
};

void TestEditPropagation::initTestCase()
{
    main = CutterTest::mainWindow();
    QVERIFY(main);
    functions = main->findChild<FunctionsWidget *>();
    comments = main->findChild<CommentsWidget *>();
    disassembly = CutterTest::findSynchronizedDock<DisassemblyWidget>(main);
    graph = CutterTest::findSynchronizedDock<GraphWidget>(main);
    QVERIFY(functions);
    QVERIFY(comments);
    QVERIFY(disassembly);
    QVERIFY(graph);

    CutterCore *core = CutterTest::core();
    const QList<FunctionDescription> all = core->getAllFunctions();
    for (const FunctionDescription &function : all) {
        if (!core->getVariables(function.offset).isEmpty()) {
            target = function.offset;
            break;
        }
    }
    QVERIFY2(target != RVA_INVALID, "no analyzed function with variables");
}

void TestEditPropagation::init()
{
    CutterTest::core()->seek(target);
    disassembly->raiseMemoryWidget();
    QTRY_VERIFY_WITH_TIMEOUT(CutterTest::disassemblyOffsets(disassembly).contains(target),
                             viewRefreshTimeoutMs);
}

void TestEditPropagation::renamedFunctionShowsInEveryView()
{
    CutterCore *core = CutterTest::core();
    const QString oldName = functionName(core, target);
    QVERIFY(!oldName.isEmpty());
    const QString newName = QStringLiteral("cutter_test_renamed_function");
    const auto restore = qScopeGuard([&]() { core->renameFunction(target, oldName); });

    core->renameFunction(target, newName);

    QTRY_VERIFY_WITH_TIMEOUT(CutterTest::disassemblyContains(disassembly, newName),
                             viewRefreshTimeoutMs);

    functions->raiseMemoryWidget();
    auto *functionList = functions->findChild<QTreeView *>();
    QVERIFY(functionList);
    QTRY_VERIFY_WITH_TIMEOUT(CutterTest::modelContains(functionList->model(), newName),
                             viewRefreshTimeoutMs);

    graph->raiseMemoryWidget();
    QTRY_VERIFY_WITH_TIMEOUT(graph->windowTitle().contains(newName), viewRefreshTimeoutMs);
}

void TestEditPropagation::commentShowsInEveryView()
{
    CutterCore *core = CutterTest::core();
    RVA address = RVA_INVALID;
    const QList<DisassemblyLine> lines = disassembly->getLines();
    for (const DisassemblyLine &line : lines) {
        if (line.offset > target && core->getFunctionStart(line.offset) == target
            && core->getCommentAt(line.offset).isEmpty()) {
            address = line.offset;
            break;
        }
    }
    QVERIFY2(address != RVA_INVALID, "no uncommented instruction shown in the function");

    const QString comment = QStringLiteral("cutter_test_comment");
    const auto restore = qScopeGuard([&]() { core->delComment(address); });

    core->setComment(address, comment);

    QTRY_VERIFY_WITH_TIMEOUT(CutterTest::disassemblyContains(disassembly, comment),
                             viewRefreshTimeoutMs);

    comments->toggleDockWidget(true);
    comments->raiseMemoryWidget();
    QAbstractItemView *commentList = CutterTest::addressableView(comments);
    QVERIFY(commentList);
    QTRY_VERIFY_WITH_TIMEOUT(CutterTest::modelContains(commentList->model(), comment),
                             viewRefreshTimeoutMs);
}

void TestEditPropagation::renamedVariableShowsInDisassembly()
{
    CutterCore *core = CutterTest::core();
    QString oldName;
    const QList<VariableDescription> variables = core->getVariables(target);
    for (const VariableDescription &variable : variables) {
        if (CutterTest::disassemblyContains(disassembly, variable.name)) {
            oldName = variable.name;
            break;
        }
    }
    if (oldName.isEmpty()) {
        QSKIP("no variable of the function is shown in the disassembly");
    }

    const QString newName = QStringLiteral("cutter_test_renamed_variable");
    const auto restore =
            qScopeGuard([&]() { core->renameFunctionVariable(oldName, newName, target); });

    core->renameFunctionVariable(newName, oldName, target);

    QTRY_VERIFY_WITH_TIMEOUT(CutterTest::disassemblyContains(disassembly, newName),
                             viewRefreshTimeoutMs);
}

void TestEditPropagation::decompilerShowsEdits()
{
    CutterCore *core = CutterTest::core();
    if (core->getDecompilers().isEmpty()) {
        QSKIP("Cutter was built without a decompiler");
    }
    auto *decompiler = CutterTest::findSynchronizedDock<DecompilerWidget>(main);
    QVERIFY(decompiler);
    auto *text = decompiler->findChild<QPlainTextEdit *>("textEdit");
    QVERIFY(text);

    const QString oldName = functionName(core, target);
    QVERIFY(!oldName.isEmpty());
    decompiler->raiseMemoryWidget();
    QTRY_VERIFY_WITH_TIMEOUT(!text->toPlainText().isEmpty(), decompileTimeoutMs);

    const QString newName = QStringLiteral("cutter_test_decompiled_function");
    const auto restore = qScopeGuard([&]() { core->renameFunction(target, oldName); });

    core->renameFunction(target, newName);

    QTRY_VERIFY_WITH_TIMEOUT(text->toPlainText().contains(newName), decompileTimeoutMs);
}

CUTTER_APP_TEST_MAIN_WITH_FILE(TestEditPropagation, CutterTest::Fixtures::helloX64())
#include "test_EditPropagation.moc"
