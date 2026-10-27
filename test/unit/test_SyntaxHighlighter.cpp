#include "SyntaxHighlighter.h"

#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include <QtTest/QtTest>

class TestSyntaxHighlighters : public QObject
{
    Q_OBJECT

private slots:
    void testFallbackKeywordsHighlighting();
    void testFallbackCommentsHighlighting();
    void testFallbackStringsHighlighting();
    void testFallbackMultiLineComments();
};

void TestSyntaxHighlighters::testFallbackKeywordsHighlighting()
{
    QTextDocument doc;
    FallbackSyntaxHighlighter highlighter(&doc);

    doc.setPlainText("void main()");
    highlighter.rehighlight();

    const QTextBlock block = doc.firstBlock();
    const auto layout = block.layout();

    const auto formats = layout->formats();
    QVERIFY(!formats.isEmpty());

    const auto formatRange = formats.first();
    QCOMPARE(formatRange.start, 0);
    QCOMPARE(formatRange.length, 4);
    QCOMPARE(formatRange.format.foreground().color(), QColor(80, 200, 215));
}

void TestSyntaxHighlighters::testFallbackCommentsHighlighting()
{
    QTextDocument doc;
    FallbackSyntaxHighlighter highlighter(&doc);

    doc.setPlainText("int x = 0; // single line comment");
    highlighter.rehighlight();

    const QTextBlock block = doc.firstBlock();
    const auto layout = block.layout();
    const auto formats = layout->formats();

    bool foundCommentFormat = false;
    for (const auto &range : formats) {
        if (range.start == 11 && range.length == 22) {
            QCOMPARE(range.format.foreground().color(), QColor(Qt::gray));
            foundCommentFormat = true;
            break;
        }
    }
    QVERIFY(foundCommentFormat);
}

void TestSyntaxHighlighters::testFallbackStringsHighlighting()
{
    QTextDocument doc;
    FallbackSyntaxHighlighter highlighter(&doc);

    doc.setPlainText("char *s = \"hello world\";");
    highlighter.rehighlight();

    const QTextBlock block = doc.firstBlock();
    const auto layout = block.layout();
    const auto formats = layout->formats();

    bool foundStringFormat = false;
    for (const auto &range : formats) {
        if (range.start == 10 && range.length == 13) {
            QCOMPARE(range.format.foreground().color(), QColor(Qt::darkGreen));
            foundStringFormat = true;
            break;
        }
    }
    QVERIFY(foundStringFormat);
}

void TestSyntaxHighlighters::testFallbackMultiLineComments()
{
    QTextDocument doc;
    FallbackSyntaxHighlighter highlighter(&doc);

    doc.setPlainText("/* start comment\n line 2\n end comment */");
    highlighter.rehighlight();

    const QTextBlock block1 = doc.firstBlock();
    const QTextBlock block2 = block1.next();
    const QTextBlock block3 = block2.next();

    QCOMPARE(block1.userState(), 1); // State 1: Inside multi-line comment
    QCOMPARE(block2.userState(), 1);
    QCOMPARE(block3.userState(), 0); // State 0: Comment closed
}

QTEST_MAIN(TestSyntaxHighlighters)
#include "test_SyntaxHighlighter.moc"