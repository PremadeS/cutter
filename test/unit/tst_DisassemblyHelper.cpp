#include "DisassemblyHelper.h"

#include <QtTest/QtTest>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>

class TestDisassemblyHelper : public QObject
{
    Q_OBJECT

private slots:
    void testFindBracketRange_data(); //NOLINT
    void testFindBracketRange();
    void testReadDisassemblyDataFromCursor();
    void testGetContextFromCursor();
};

void TestDisassemblyHelper::testFindBracketRange_data() //NOLINT
{
    QTest::addColumn<QString>("line");
    QTest::addColumn<int>("posInLine");
    QTest::addColumn<bool>("expectedFound");
    QTest::addColumn<int>("expectedStart");
    QTest::addColumn<int>("expectedLength");
    QTest::addColumn<QString>("expectedContent");

    // Cursor inside single brackets
    QTest::newRow("inside_brackets")
        << "mov eax, [0x1000]" << 12 << true << 9 << 8 << "[0x1000]";

    // Cursor on the open bracket
    QTest::newRow("at_open_bracket")
        << "mov eax, [0x1000]" << 9 << true << 9 << 8 << "[0x1000]";

    // Cursor outside brackets
    QTest::newRow("outside_brackets")
        << "mov eax, [0x1000]" << 3 << false << 0 << 0 << "";

    // Multiple bracket pairs, cursor inside second pair
    QTest::newRow("second_bracket_pair")
        << "lea rax, [rbx + [rcx]]" << 18 << true << 16 << 5 << "[rcx]";

    // Unmatched bracket pair
    QTest::newRow("unmatched_bracket")
        << "mov eax, [0x1000" << 11 << false << 0 << 0 << "";
}

void TestDisassemblyHelper::testFindBracketRange()
{
    QFETCH(QString, line);
    QFETCH(int, posInLine);
    QFETCH(bool, expectedFound);
    QFETCH(int, expectedStart);
    QFETCH(int, expectedLength);
    QFETCH(QString, expectedContent);

    const auto res = DisassemblyHelper::findBracketRange(line, posInLine);

    QCOMPARE(res.found, expectedFound);
    if (expectedFound) {
        QCOMPARE(res.start, expectedStart);
        QCOMPARE(res.length, expectedLength);
        QCOMPARE(res.content, expectedContent);
    }
}

void TestDisassemblyHelper::testReadDisassemblyDataFromCursor()
{
    QTextDocument doc;
    QTextCursor cursor(&doc);

    // Write text to the document
    cursor.insertText("0x00401000: mov eax, 1\n");

    // Prepare line and user data
    DisassemblyLine line;
    line.offset = 0x00401000;
    line.arrow = 0x00401050;

    auto *userData = new DisassemblyTextBlockUserData(line);
    cursor.block().setUserData(userData);

    // Verify UserData reads back correctly via DisassemblyHelper
    QCOMPARE(DisassemblyHelper::readDisassemblyOffset(cursor), RVA(0x00401000));
    QCOMPARE(DisassemblyHelper::readDisassemblyArrow(cursor), RVA(0x00401050));
}

void TestDisassemblyHelper::testGetContextFromCursor()
{
    QTextDocument doc;
    QTextCursor cursor(&doc);

    cursor.insertText("mov eax, [0x402000]");

    DisassemblyLine line;
    line.offset = 0x00401000;
    line.arrow = RVA_INVALID;

    cursor.block().setUserData(new DisassemblyTextBlockUserData(line));

    // Place cursor over bracketed memory address: "[0x402000]"
    cursor.setPosition(12);

    const auto ctx = DisassemblyHelper::getContextFromCursor(cursor);

    QCOMPARE(ctx.word, QString("[0x402000]"));
    QCOMPARE(ctx.line, QString("mov eax, [0x402000]"));
    QCOMPARE(ctx.offset, RVA(0x00401000));
    QCOMPARE(ctx.arrow, RVA_INVALID);
}

QTEST_MAIN(TestDisassemblyHelper)
#include "tst_DisassemblyHelper.moc"
