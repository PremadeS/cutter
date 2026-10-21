#include "SelectionHighlight.h"

#include <QPlainTextEdit>
#include <QTest>
#include <QTextCursor>
#include <QTextDocument>

class TestSelectionHighlight : public QObject
{
    Q_OBJECT

private slots:
    void testEmptyWord();
    void testSameWordHighlight();
    void testBraceMatchingForward();
    void testBraceMatchingBackward();
    void testLineHighlights();
};

void TestSelectionHighlight::testEmptyWord()
{
    QPlainTextEdit edit("int main() { return 0; }");
    auto selections = createSameWordsSelections(&edit, "");
    QVERIFY(selections.isEmpty());
}

void TestSelectionHighlight::testSameWordHighlight()
{
    QPlainTextEdit edit("int var = 1;\nint domain = 2;\nvar = var + 1;");

    // Search for occurrences of whole word "var"
    auto selections = createSameWordsSelections(&edit, "var");

    // Must match "var" (line 1) and two "var"s (line 3), but NOT "domain"
    QCOMPARE(selections.size(), 3);
}

void TestSelectionHighlight::testBraceMatchingForward()
{
    QPlainTextEdit edit("void foo() {\n    if (x) {\n        return;\n    }\n}");

    // Position cursor at opening brace '{' of foo()
    QTextCursor cursor = edit.document()->find("{", 0);
    QVERIFY(!cursor.isNull());
    cursor.setPosition(cursor.selectionStart());
    edit.setTextCursor(cursor);

    auto selections = createSameWordsSelections(&edit, "{");

    // Expect 2 selections: starting brace and the matching outer closing brace
    QCOMPARE(selections.size(), 2);
    QCOMPARE(selections[1].cursor.selectedText(), QString("}"));
}

void TestSelectionHighlight::testBraceMatchingBackward()
{
    QPlainTextEdit edit("void foo() {\n    if (x) {\n        return;\n    }\n}");

    // Find the last closing brace '}' and position cursor right before it
    QTextCursor cursor = edit.document()->find("}", edit.document()->characterCount(),
                                               QTextDocument::FindBackward);
    QVERIFY(!cursor.isNull());
    cursor.setPosition(cursor.selectionStart());
    edit.setTextCursor(cursor);

    auto selections = createSameWordsSelections(&edit, "}");

    // Expect 2 selections: closing brace and the matching outer opening brace
    QCOMPARE(selections.size(), 2);
    QCOMPARE(selections[1].cursor.selectedText(), QString("{"));
}

void TestSelectionHighlight::testLineHighlights()
{
    QPlainTextEdit edit("Line 1\nLine 2");
    QTextCursor cursor = edit.textCursor();

    auto lineSel = createLineHighlightSelection(cursor);
    QVERIFY(lineSel.format.property(QTextFormat::FullWidthSelection).toBool());

    auto pcSel = createLineHighlightPC(cursor);
    QVERIFY(pcSel.format.property(QTextFormat::FullWidthSelection).toBool());

    auto bpSel = createLineHighlightBP(cursor);
    QVERIFY(bpSel.format.property(QTextFormat::FullWidthSelection).toBool());
}

QTEST_MAIN(TestSelectionHighlight)
#include "tst_SelectionHighlight.moc"