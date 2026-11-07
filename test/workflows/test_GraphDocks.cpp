/**
 * @file test_GraphDocks.cpp
 * @brief The graph docks besides the Graph itself show the current analysis.
 *
 * - The call graphs didn't refresh when a function was defined or undefined.
 * - Rizin graph nodes with a body showed `title/nbody` on one line: the separator was "/n" and
 *   nodes were drawn as a single line.
 * - The Graph Overview's rectangle was placed for the previous function's graph when the overview
 *   was hidden while the Graph switched function.
 */

#include "Fixtures.h"
#include "TestEnv.h"
#include "ViewHelpers.h"

#include "widgets/CallGraph.h"
#include "widgets/OverviewView.h"
#include "widgets/OverviewWidget.h"
#include "widgets/RizinGraphWidget.h"

#include <QComboBox>
#include <QScopeGuard>
#include <QTest>

#include <algorithm>

namespace {

constexpr int viewRefreshTimeoutMs = 10000;
const QString globalCallGraphType = QStringLiteral("GlobalCallgraph");
constexpr char customGraphType = 'g';

int blockCountWithin(const GraphView *view)
{
    return static_cast<int>(view->getBlocks().size());
}

} // namespace

class TestGraphDocks : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void rizinGraphShowsNodeBodies();
    void overviewRectangleFollowsWhileHidden();
    void globalCallGraphFollowsDefinedFunctions();

private:
    MainWindow *main = nullptr;
    GraphWidget *graph = nullptr;
    FunctionDescription mainFunction;
    RVA biggest = RVA_INVALID;
    RVA smallest = RVA_INVALID;
};

void TestGraphDocks::initTestCase()
{
    main = CutterTest::mainWindow();
    QVERIFY(main);
    graph = CutterTest::findSynchronizedDock<GraphWidget>(main);
    QVERIFY(graph);

    const QList<FunctionDescription> functions = CutterTest::core()->getAllFunctions();
    RVA mostBlocks = 0;
    RVA fewestBlocks = RVA_INVALID;
    for (const FunctionDescription &function : functions) {
        if (function.name == QLatin1String("main") || function.name == QLatin1String("sym.main")) {
            mainFunction = function;
        }
        if (function.nbbs > mostBlocks) {
            mostBlocks = function.nbbs;
            biggest = function.offset;
        }
        if (function.nbbs < fewestBlocks) {
            fewestBlocks = function.nbbs;
            smallest = function.offset;
        }
    }
    QVERIFY2(mainFunction.offset != RVA_INVALID && !mainFunction.name.isEmpty(), "no main");
    QVERIFY2(mostBlocks > fewestBlocks, "all functions have the same number of blocks");
}

void TestGraphDocks::rizinGraphShowsNodeBodies()
{
    CutterCore *core = CutterTest::core();
    auto *widget = main->findChild<RizinGraphWidget *>();
    QVERIFY(widget);
    auto *view = widget->findChild<GenericRizinGraphView *>();
    auto *type = widget->findChild<QComboBox *>(QStringLiteral("graphType"));
    QVERIFY(view);
    QVERIFY(type);

    const auto restore = qScopeGuard([&]() {
        core->cmd(QStringLiteral("ag-"));
        widget->hide();
    });
    core->cmd(QStringLiteral("ag-"));
    core->cmd(QStringLiteral("agn titleonly"));
    core->cmd(QStringLiteral("agn withbody body"));
    core->cmd(QStringLiteral("age withbody titleonly"));

    widget->show();
    widget->raise();
    const int index = type->findData(QChar(customGraphType));
    QVERIFY(index >= 0);
    type->setCurrentIndex(index);
    view->refreshView();
    QTRY_COMPARE_WITH_TIMEOUT(blockCountWithin(view), 2, viewRefreshTimeoutMs);

    int shortest = INT_MAX;
    int tallest = 0;
    for (const auto &it : view->getBlocks()) {
        shortest = std::min(shortest, it.second.height);
        tallest = std::max(tallest, it.second.height);
    }
    QVERIFY2(tallest > shortest, "the node with a body isn't taller than the one without");
}

void TestGraphDocks::overviewRectangleFollowsWhileHidden()
{
    auto *overview = main->findChild<OverviewWidget *>();
    QVERIFY(overview);
    auto *overviewView = overview->findChild<OverviewView *>();
    QVERIFY(overviewView);
    DisassemblerGraphView *view = graph->getGraphView();

    // The overview position of the logical point in the middle of the Graph view
    const auto graphCenterInOverview = [&]() {
        const QPointF center = QPointF(view->getViewOffset())
                + QPointF(view->viewport()->rect().center()) / view->getViewScale();
        return overviewView->logicalToViewCoordinates(center.toPoint());
    };

    graph->raiseMemoryWidget();
    CutterTest::core()->seek(biggest);
    QTRY_VERIFY_WITH_TIMEOUT(CutterTest::graphHasBlock(graph, biggest), viewRefreshTimeoutMs);
    overview->show();
    QTRY_VERIFY_WITH_TIMEOUT(overviewView->getRangeRect().contains(graphCenterInOverview()),
                             viewRefreshTimeoutMs);

    overview->hide();
    CutterTest::core()->seek(smallest);
    QTRY_VERIFY_WITH_TIMEOUT(CutterTest::graphHasBlock(graph, smallest), viewRefreshTimeoutMs);
    overview->show();
    QTRY_VERIFY2_WITH_TIMEOUT(overviewView->getRangeRect().contains(graphCenterInOverview()),
                              "the overview rectangle isn't where the Graph is",
                              viewRefreshTimeoutMs);
}

void TestGraphDocks::globalCallGraphFollowsDefinedFunctions()
{
    CutterCore *core = CutterTest::core();
    CallGraphWidget *widget = nullptr;
    const QList<CallGraphWidget *> widgets = main->findChildren<CallGraphWidget *>();
    for (CallGraphWidget *candidate : widgets) {
        if (candidate->getWidgetType() == globalCallGraphType) {
            widget = candidate;
        }
    }
    QVERIFY(widget);
    auto *view = widget->findChild<CallGraphView *>();
    QVERIFY(view);

    widget->raiseMemoryWidget();
    const RVA start = mainFunction.offset;
    QTRY_VERIFY_WITH_TIMEOUT(view->getBlocks().count(start) > 0, viewRefreshTimeoutMs);

    // main isn't called by anything, so undefining it removes its block
    const auto restore = qScopeGuard([&]() {
        if (!core->functionIn(start)) {
            core->createFunctionAt(start, mainFunction.name);
        }
    });
    core->delFunction(start);
    QTRY_VERIFY2_WITH_TIMEOUT(view->getBlocks().count(start) == 0,
                              "an undefined function is still in the global call graph",
                              viewRefreshTimeoutMs);

    core->createFunctionAt(start, mainFunction.name);
    QTRY_VERIFY2_WITH_TIMEOUT(view->getBlocks().count(start) > 0,
                              "a defined function isn't in the global call graph",
                              viewRefreshTimeoutMs);
}

CUTTER_APP_TEST_MAIN_WITH_FILE(TestGraphDocks, CutterTest::Fixtures::helloX64())
#include "test_GraphDocks.moc"
