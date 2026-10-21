#include "common/BinaryTrees.h"

#include <QTest>

class TestBinaryTrees : public QObject
{
    Q_OBJECT

private slots:
    void testPointSetMinTreeBasic();
    void testPointSetMinTreeLessThanQueries();
    void testRangeAssignMaxTree();
    void testMinMaxAccumulateTree();
};

void TestBinaryTrees::testPointSetMinTreeBasic()
{
    // Initialize tree with 8 leaves and initial value 100
    PointSetMinTree tree(8, 100);

    for (size_t i = 0; i < 8; ++i) {
        QCOMPARE(tree.valueAtPoint(i), 100);
    }

    // Set point values
    tree.set(3, 15);
    tree.set(5, 42);

    QCOMPARE(tree.valueAtPoint(3), 15);
    QCOMPARE(tree.valueAtPoint(5), 42);
    QCOMPARE(tree.valueAtPoint(0), 100);
}

void TestBinaryTrees::testPointSetMinTreeLessThanQueries()
{
    // Tree array representation: [10, 20, 5, 15, 30, 2, 25, 40]
    PointSetMinTree tree(8, 100);
    const std::vector<int> values = { 10, 20, 5, 15, 30, 2, 25, 40 };
    for (size_t i = 0; i < values.size(); ++i) {
        tree.set(i, values[i]);
    }

    // Test rightMostLessThan:
    // Search in range [0; 4] for value less than 15 (values: 10, 20, 5, 15, 30) -> elements < 15 are 10 (idx 0), 5 (idx 2). Rightmost is 2.
    QCOMPARE(tree.rightMostLessThan(4, 15), 2);

    // Search in range [0; 7] for value less than 10 -> elements < 10 are 5 (idx 2), 2 (idx 5). Rightmost is 5.
    QCOMPARE(tree.rightMostLessThan(7, 10), 5);

    // Search in range [0; 1] for value less than 5 -> no such element
    QCOMPARE(tree.rightMostLessThan(1, 5), -1);

    // Test leftMostLessThan:
    // Search in range [0; 7] for value less than 15 -> elements < 15 are 10 (idx 0), 5 (idx 2), 2 (idx 5). Leftmost is 0.
    QCOMPARE(tree.leftMostLessThan(0, 15), 0);

    // Search in range [3; 7] for value less than 10 (values: 15, 30, 2, 25, 40) -> element < 10 is 2 (idx 5).
    QCOMPARE(tree.leftMostLessThan(3, 10), 5);

    // Search in range [6; 7] for value less than 10 (values: 25, 40) -> no such element
    QCOMPARE(tree.leftMostLessThan(6, 10), -1);
}

void TestBinaryTrees::testRangeAssignMaxTree()
{
    // Create RangeAssignMaxTree with 10 elements initialized to 0
    RangeAssignMaxTree tree(10, 0);

    QCOMPARE(tree.rangeMaximum(0, 10), 0);

    // Set range [2, 6) to 10
    tree.setRange(2, 6, 10);
    QCOMPARE(tree.rangeMaximum(0, 2), 0);
    QCOMPARE(tree.rangeMaximum(2, 6), 10);
    QCOMPARE(tree.rangeMaximum(0, 10), 10);

    // Overlapping update: set range [4, 8) to 20
    tree.setRange(4, 8, 20);
    QCOMPARE(tree.rangeMaximum(2, 4), 10);
    QCOMPARE(tree.rangeMaximum(4, 8), 20);
    QCOMPARE(tree.rangeMaximum(2, 8), 20);
    QCOMPARE(tree.rangeMaximum(8, 10), 0);
}

void TestBinaryTrees::testMinMaxAccumulateTree()
{
    // Test example case provided in class documentation
    MinMaxAccumulateTree<int> tree(30);

    tree.updateRange(1, 5, 10);
    auto res1 = tree.rangeMinMax(0, 20);
    QCOMPARE(res1.first, 10);
    QCOMPARE(res1.second, 10);

    tree.updateRange(4, 6, 15);
    tree.updateRange(3, 10, 20);

    auto res2 = tree.rangeMinMax(0, 20);
    QCOMPARE(res2.first, 10);
    QCOMPARE(res2.second, 20);

    auto res3 = tree.rangeMinMax(1, 3);
    QCOMPARE(res3.first, 10);
    QCOMPARE(res3.second, 10);

    auto res4 = tree.rangeMinMax(5, 8);
    QCOMPARE(res4.first, 15);
    QCOMPARE(res4.second, 20);
}

QTEST_MAIN(TestBinaryTrees)
#include "tst_BinaryTrees.moc"
