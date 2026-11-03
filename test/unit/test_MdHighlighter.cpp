#include "common/MdHighlighter.h"

#include <QColor>
#include <QFont>
#include <QTest>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>

class TestMdHighlighter : public QObject
{
    Q_OBJECT

private slots:
    void testHeaders();
    void testBoldAndItalics();
    void testComments();
    void testPlainText();
    void testMultilineDocument();
};

void TestMdHighlighter::testHeaders()
{
    QTextDocument doc;
    MdHighlighter highlighter(&doc);

    doc.setPlainText("# Section Header");
    highlighter.rehighlight();

    QTextBlock block = doc.firstBlock();
    const auto formats = block.layout()->formats();

    QCOMPARE(formats.size(), 1);
    QCOMPARE(formats[0].start, 0);
    QCOMPARE(formats[0].length, 16);
    QCOMPARE(formats[0].format.foreground().color(), QColor(Qt::gray));
    QCOMPARE(formats[0].format.fontWeight(), static_cast<int>(QFont::Bold));
}

void TestMdHighlighter::testBoldAndItalics()
{
    QTextDocument doc;
    MdHighlighter highlighter(&doc);

    doc.setPlainText("Text with **bold** word");
    highlighter.rehighlight();

    QTextBlock block = doc.firstBlock();
    const auto formats = block.layout()->formats();

    QCOMPARE(formats.size(), 1);
    QCOMPARE(formats[0].start, 10);
    QCOMPARE(formats[0].length, 8); // **bold**
    QCOMPARE(formats[0].format.foreground().color(), QColor(Qt::gray));
    QCOMPARE(formats[0].format.fontWeight(), static_cast<int>(QFont::Bold));
}

void TestMdHighlighter::testComments()
{
    QTextDocument doc;
    MdHighlighter highlighter(&doc);

    doc.setPlainText("; This is a single line comment");
    highlighter.rehighlight();

    QTextBlock block = doc.firstBlock();
    const auto formats = block.layout()->formats();

    QCOMPARE(formats.size(), 1);
    QCOMPARE(formats[0].start, 0);
    QCOMPARE(formats[0].length, 31);
    QCOMPARE(formats[0].format.foreground().color(), QColor(Qt::darkGreen));
    QCOMPARE(formats[0].format.fontWeight(), static_cast<int>(QFont::Bold));
}

void TestMdHighlighter::testPlainText()
{
    QTextDocument doc;
    MdHighlighter highlighter(&doc);

    doc.setPlainText("Just standard unformatted text.");
    highlighter.rehighlight();

    QTextBlock block = doc.firstBlock();
    const auto formats = block.layout()->formats();

    QCOMPARE(formats.size(), 0);
}

void TestMdHighlighter::testMultilineDocument()
{
    QTextDocument doc;
    MdHighlighter highlighter(&doc);

    doc.setPlainText("## Header\nNormal text line\n; Assembly comment");
    highlighter.rehighlight();

    QTextBlock block1 = doc.findBlockByNumber(0);
    const auto formats1 = block1.layout()->formats();
    QCOMPARE(formats1.size(), 1);
    QCOMPARE(formats1[0].format.foreground().color(), QColor(Qt::gray));

    QTextBlock block2 = doc.findBlockByNumber(1);
    const auto formats2 = block2.layout()->formats();
    QCOMPARE(formats2.size(), 0);

    QTextBlock block3 = doc.findBlockByNumber(2);
    const auto formats3 = block3.layout()->formats();
    QCOMPARE(formats3.size(), 1);
    QCOMPARE(formats3[0].format.foreground().color(), QColor(Qt::darkGreen));
}

QTEST_MAIN(TestMdHighlighter)
#include "test_MdHighlighter.moc"