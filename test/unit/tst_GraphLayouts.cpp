#include "widgets/GraphGridLayout.h"
#include "widgets/GraphHorizontalAdapter.h"
#if CUTTER_ENABLE_GRAPHVIZ
#    include "widgets/GraphvizLayout.h"
#endif

#include <QtTest/QtTest>

#include <memory>

class TestGraphLayouts : public QObject
{
    Q_OBJECT

private slots:
    // GraphGridLayout
    void testEmptyGraph();
    void testSingleBlock_data(); // NOLINT
    void testSingleBlock();
    void testLinearFlow();
    void testBranchingColumns_data(); // NOLINT
    void testBranchingColumns();
    void testCyclicFlow();

    // GraphHorizontalAdapter Tests
    void testHorizontalAdapterSwapsCoordinatesAndArrowDirections();

    // GraphvizLayout Tests
#if CUTTER_ENABLE_GRAPHVIZ
    void testGraphvizLayoutLinearFlow();
#endif
};

void TestGraphLayouts::testEmptyGraph()
{
    GraphGridLayout gridLayout(GraphGridLayout::LayoutType::Medium);
    GraphLayout::Graph emptyGraph;
    int width = 0;
    int height = 0;

    gridLayout.calculateLayout(emptyGraph, 0x1000, width, height);

    QCOMPARE(width, 0);
    QCOMPARE(height, 0);
}

void TestGraphLayouts::testSingleBlock_data() // NOLINT
{
    QTest::addColumn<int>("blockWidth");
    QTest::addColumn<int>("blockHeight");

    QTest::newRow("standard_instruction_block") << 120 << 60;
    QTest::newRow("wide_instruction_block") << 300 << 150;
}

void TestGraphLayouts::testSingleBlock()
{
    QFETCH(int, blockWidth);
    QFETCH(int, blockHeight);

    GraphGridLayout gridLayout(GraphGridLayout::LayoutType::Medium);
    GraphLayout::Graph graph;

    GraphLayout::GraphBlock block;
    block.entry = 0x00401000;
    block.width = blockWidth;
    block.height = blockHeight;
    graph[block.entry] = block;

    int totalWidth = 0;
    int totalHeight = 0;

    gridLayout.calculateLayout(graph, 0x00401000, totalWidth, totalHeight);

    // Root block coordinates include default edgeHorizontalSpacing (10px) & edgeVerticalSpacing
    // (10px)
    QCOMPARE(graph[0x00401000].x, 10);
    QCOMPARE(graph[0x00401000].y, 10);
    QCOMPARE(totalWidth, blockWidth + 20); // Horizontal margin (10 left + 10 right)
    QCOMPARE(totalHeight, blockHeight + 20); // Vertical margin (10 top + 10 bottom)
}

void TestGraphLayouts::testLinearFlow()
{
    GraphGridLayout gridLayout(GraphGridLayout::LayoutType::Medium);
    GraphLayout::Graph graph;

    GraphLayout::GraphBlock blockA;
    blockA.entry = 0x1000;
    blockA.width = 100;
    blockA.height = 50;
    blockA.edges.emplace_back(0x2000);

    GraphLayout::GraphBlock blockB;
    blockB.entry = 0x2000;
    blockB.width = 100;
    blockB.height = 50;

    graph[0x1000] = blockA;
    graph[0x2000] = blockB;

    int totalWidth = 0;
    int totalHeight = 0;

    gridLayout.calculateLayout(graph, 0x1000, totalWidth, totalHeight);

    QVERIFY(graph[0x2000].y > graph[0x1000].y);
    QVERIFY(!graph[0x1000].edges.empty());
    QVERIFY(!graph[0x1000].edges[0].polyline.isEmpty());
}

void TestGraphLayouts::testBranchingColumns_data() // NOLINT
{
    QTest::addColumn<GraphGridLayout::LayoutType>("layoutType");

    QTest::newRow("medium_grid_type") << GraphGridLayout::LayoutType::Medium;
    QTest::newRow("wide_grid_type") << GraphGridLayout::LayoutType::Wide;
    QTest::newRow("narrow_grid_type") << GraphGridLayout::LayoutType::Narrow;
}

void TestGraphLayouts::testBranchingColumns()
{
    QFETCH(GraphGridLayout::LayoutType, layoutType);

    GraphGridLayout gridLayout(layoutType);
    GraphLayout::Graph graph;

    GraphLayout::GraphBlock blockA;
    blockA.entry = 0x1000;
    blockA.width = 100;
    blockA.height = 40;
    blockA.edges.emplace_back(0x2000);
    blockA.edges.emplace_back(0x3000);

    GraphLayout::GraphBlock blockB;
    blockB.entry = 0x2000;
    blockB.width = 100;
    blockB.height = 40;

    GraphLayout::GraphBlock blockC;
    blockC.entry = 0x3000;
    blockC.width = 100;
    blockC.height = 40;

    graph[0x1000] = blockA;
    graph[0x2000] = blockB;
    graph[0x3000] = blockC;

    int totalWidth = 0;
    int totalHeight = 0;

    gridLayout.calculateLayout(graph, 0x1000, totalWidth, totalHeight);

    QVERIFY(graph[0x2000].y > graph[0x1000].y);
    QVERIFY(graph[0x3000].y > graph[0x1000].y);
    QVERIFY(graph[0x2000].x != graph[0x3000].x);
}

void TestGraphLayouts::testCyclicFlow()
{
    GraphGridLayout gridLayout(GraphGridLayout::LayoutType::Medium);
    GraphLayout::Graph graph;

    GraphLayout::GraphBlock blockA;
    blockA.entry = 0x1000;
    blockA.width = 80;
    blockA.height = 40;
    blockA.edges.emplace_back(0x2000);

    GraphLayout::GraphBlock blockB;
    blockB.entry = 0x2000;
    blockB.width = 80;
    blockB.height = 40;
    blockB.edges.emplace_back(0x1000);

    graph[0x1000] = blockA;
    graph[0x2000] = blockB;

    int totalWidth = 0;
    int totalHeight = 0;

    gridLayout.calculateLayout(graph, 0x1000, totalWidth, totalHeight);

    QVERIFY(totalWidth > 0);
    QVERIFY(totalHeight > 0);
}

void TestGraphLayouts::testHorizontalAdapterSwapsCoordinatesAndArrowDirections()
{
    // Build vertical grid layout wrapped in horizontal adapter
    auto baseGrid = std::make_unique<GraphGridLayout>(GraphGridLayout::LayoutType::Medium);
    GraphHorizontalAdapter adapter(std::move(baseGrid));

    GraphLayout::Graph graph;

    GraphLayout::GraphBlock blockA;
    blockA.entry = 0x1000;
    blockA.width = 100;
    blockA.height = 50;
    blockA.edges.emplace_back(0x2000);

    GraphLayout::GraphBlock blockB;
    blockB.entry = 0x2000;
    blockB.width = 100;
    blockB.height = 50;

    graph[0x1000] = blockA;
    graph[0x2000] = blockB;

    int totalWidth = 0;
    int totalHeight = 0;

    adapter.calculateLayout(graph, 0x1000, totalWidth, totalHeight);

    // In horizontal layout, block B should be placed to the right of block A (x increases)
    QVERIFY(graph[0x2000].x > graph[0x1000].x);

    // Verify arrow direction mapped from Down -> Right
    QCOMPARE(graph[0x1000].edges[0].arrow, GraphLayout::GraphEdge::Right);
}

#if CUTTER_ENABLE_GRAPHVIZ
void TestGraphLayouts::testGraphvizLayoutLinearFlow()
{
    GraphvizLayout layout(GraphvizLayout::LayoutType::DotPolyline, GraphvizLayout::Direction::TB);
    GraphLayout::Graph graph;

    GraphLayout::GraphBlock blockA;
    blockA.entry = 0x1000;
    blockA.width = 120;
    blockA.height = 60;
    blockA.edges.emplace_back(0x2000);

    GraphLayout::GraphBlock blockB;
    blockB.entry = 0x2000;
    blockB.width = 120;
    blockB.height = 60;

    graph[0x1000] = blockA;
    graph[0x2000] = blockB;

    int totalWidth = 0;
    int totalHeight = 0;

    layout.calculateLayout(graph, 0x1000, totalWidth, totalHeight);

    // verify Graphviz assigned non-zero canvas dimensions and positions
    QVERIFY(totalWidth > 0);
    QVERIFY(totalHeight > 0);
    QVERIFY(!graph[0x1000].edges[0].polyline.isEmpty());
}
#endif

QTEST_MAIN(TestGraphLayouts)
#include "tst_GraphLayouts.moc"