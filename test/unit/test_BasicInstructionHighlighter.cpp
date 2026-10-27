#include "common/BasicInstructionHighlighter.h"

#include <QTest>

class TestInstructionHighlighters : public QObject
{
    Q_OBJECT

private slots:
    void testBasicLookup();
    void testPartialClearSplitsInstruction();
    void testFullOverlapClear();
};

void TestInstructionHighlighters::testBasicLookup()
{
    BasicInstructionHighlighter highlighter;

    // Highlight 10 bytes at 0x1000
    highlighter.highlight(0x1000, 10, Qt::red);

    // Lookups inside the range [0x1000, 0x100A)
    const auto *biStart = highlighter.getBasicInstruction(0x1000);
    QVERIFY(biStart != nullptr);
    QCOMPARE(biStart->address, RVA(0x1000));
    QCOMPARE(biStart->size, RVA(10));

    const auto *biMid = highlighter.getBasicInstruction(0x1005);
    QVERIFY(biMid != nullptr);

    // Lookup past the end boundary
    const auto *biEnd = highlighter.getBasicInstruction(0x100A);
    QVERIFY(biEnd == nullptr);
}

void TestInstructionHighlighters::testPartialClearSplitsInstruction()
{
    BasicInstructionHighlighter highlighter;

    // Highlight 0x20 bytes at 0x1000: [0x1000, 0x1020)
    highlighter.highlight(0x1000, 0x20, Qt::green);

    // Punch a hole in the middle: clear [0x1008, 0x1010) (size 0x08)
    highlighter.clear(0x1008, 0x08);

    // Left segment should remain: [0x1000, 0x1008)
    const auto *leftSegment = highlighter.getBasicInstruction(0x1004);
    QVERIFY(leftSegment != nullptr);
    QCOMPARE(leftSegment->address, RVA(0x1000));
    QCOMPARE(leftSegment->size, RVA(0x08));

    // Cleared region should return nullptr
    QVERIFY(highlighter.getBasicInstruction(0x1008) == nullptr);
    QVERIFY(highlighter.getBasicInstruction(0x100F) == nullptr);

    // Right segment should remain: [0x1010, 0x1020)
    const auto *rightSegment = highlighter.getBasicInstruction(0x1010);
    QVERIFY(rightSegment != nullptr);
    QCOMPARE(rightSegment->address, RVA(0x1010));
    QCOMPARE(rightSegment->size, RVA(0x10));
}

void TestInstructionHighlighters::testFullOverlapClear()
{
    BasicInstructionHighlighter highlighter;

    highlighter.highlight(0x1000, 0x05, Qt::blue);
    highlighter.highlight(0x1010, 0x05, Qt::yellow);

    // Clear range encompassing both instructions: [0x0F00, 0x2000)
    highlighter.clear(0x0F00, 0x1100);

    QVERIFY(highlighter.getBasicInstruction(0x1000) == nullptr);
    QVERIFY(highlighter.getBasicInstruction(0x1010) == nullptr);
}

QTEST_MAIN(TestInstructionHighlighters)
#include "test_BasicInstructionHighlighter.moc"
