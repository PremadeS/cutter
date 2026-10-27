#include "Fixtures.h"
#include "TestEnv.h"

#include "common/AddressableItemModel.h"
#include "common/CutterSeekable.h"
#include "core/MainWindow.h"
#include "widgets/DisassemblerGraphView.h"
#include "widgets/DisassemblyWidget.h"
#include "widgets/FunctionsWidget.h"
#include "widgets/GraphWidget.h"

#include <QSet>
#include <QTest>
#include <QTreeView>

namespace {

constexpr int viewRefreshTimeoutMs = 10000;

template<class Dock>
Dock *findSynchronizedDock(MainWindow *main)
{
    const QList<Dock *> docks = main->findChildren<Dock *>();
    for (Dock *dock : docks) {
        if (dock->getSeekable() && dock->getSeekable()->isSynchronized()) {
            return dock;
        }
    }
    return nullptr;
}

QSet<RVA> lineOffsets(DisassemblyWidget *disassembly)
{
    QSet<RVA> offsets;
    const QList<DisassemblyLine> lines = disassembly->getLines();
    for (const DisassemblyLine &line : lines) {
        offsets.insert(line.offset);
    }
    return offsets;
}

bool graphHasBlock(GraphWidget *graph, RVA address)
{
    return graph->getGraphView()->getBlocks().count(address) > 0;
}

} // namespace

class TestNavigation : public QObject
{
    Q_OBJECT

private slots:
    void activatingFunctionSyncsDisassemblyAndGraph();
};

void TestNavigation::activatingFunctionSyncsDisassemblyAndGraph()
{
    MainWindow *main = CutterTest::mainWindow();
    QVERIFY(main);

    auto *functions = main->findChild<FunctionsWidget *>();
    auto *disassembly = findSynchronizedDock<DisassemblyWidget>(main);
    auto *graph = findSynchronizedDock<GraphWidget>(main);
    QVERIFY(functions);
    QVERIFY(disassembly);
    QVERIFY(graph);

    disassembly->raiseMemoryWidget();
    QTRY_VERIFY(!disassembly->getLines().isEmpty());
    const QSet<RVA> visibleBefore = lineOffsets(disassembly);

    functions->raiseMemoryWidget();
    auto *view = functions->findChild<QTreeView *>();
    QVERIFY(view);
    QTRY_VERIFY(view->model()->rowCount() > 0);
    auto *addressable = dynamic_cast<AddressableItemModelI *>(view->model());
    QVERIFY(addressable);

    QModelIndex targetIndex;
    RVA target = RVA_INVALID;
    for (int row = 0; row < view->model()->rowCount(); ++row) {
        const QModelIndex index = view->model()->index(row, 0);
        const RVA address = addressable->address(index);
        if (address != RVA_INVALID && address != CutterTest::core()->getOffset()
            && !visibleBefore.contains(address) && !graphHasBlock(graph, address)) {
            targetIndex = index;
            target = address;
            break;
        }
    }
    QVERIFY2(targetIndex.isValid(), "no function outside the currently displayed code");

    emit view->activated(targetIndex);
    QCOMPARE(CutterTest::core()->getOffset(), target);

    disassembly->raiseMemoryWidget();
    QTRY_VERIFY_WITH_TIMEOUT(lineOffsets(disassembly).contains(target), viewRefreshTimeoutMs);

    graph->raiseMemoryWidget();
    QTRY_VERIFY_WITH_TIMEOUT(graphHasBlock(graph, target), viewRefreshTimeoutMs);
}

CUTTER_APP_TEST_MAIN_WITH_FILE(TestNavigation, CutterTest::Fixtures::helloX64())
#include "test_Navigation.moc"
