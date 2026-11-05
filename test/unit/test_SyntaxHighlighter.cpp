/**
 * @file test_SyntaxHighlighter.cpp
 * @brief Token highlighting of the fallback C highlighter.
 *
 * The decompiler and graph views use FallbackSyntaxHighlighter when KSyntaxHighlighting is not
 * available. A broken rule leaves keywords plain, colors parts of identifiers, or lets a comment
 * bleed into the code after it.
 */

#include "common/SyntaxHighlighter.h"

#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include <QtTest/QtTest>

namespace {

class Highlighted
{
public:
    explicit Highlighted(const QString &text) : highlighter(&document)
    {
        document.setPlainText(text);
        highlighter.rehighlight();
    }

    QColor colorAt(int position) const
    {
        const QTextBlock block = document.findBlock(position);
        const int offset = position - block.position();
        for (const QTextLayout::FormatRange &range : block.layout()->formats()) {
            if (offset >= range.start && offset < range.start + range.length) {
                return range.format.foreground().color();
            }
        }
        return {};
    }

    const FallbackSyntaxHighlighter &rules() const { return highlighter; }

    bool spanHasColor(int start, int length, const QColor &color) const
    {
        for (int position = start; position < start + length; ++position) {
            if (colorAt(position) != color) {
                return false;
            }
        }
        return true;
    }

private:
    QTextDocument document;
    FallbackSyntaxHighlighter highlighter;
};

} // namespace

class TestSyntaxHighlighter : public QObject
{
    Q_OBJECT

private slots:
    void everyKeywordIsHighlighted_data();
    void everyKeywordIsHighlighted();
    void keywordInsideIdentifierIsNotHighlighted_data();
    void keywordInsideIdentifierIsNotHighlighted();
    void functionCallIsHighlighted();
    void lineCommentCoversRestOfLine();
    void stringLiteralIsHighlighted();
    void blockCommentSpansLinesAndEnds();
};

void TestSyntaxHighlighter::everyKeywordIsHighlighted_data()
{
    QTest::addColumn<QString>("keyword");
    const FallbackSyntaxHighlighter highlighter;
    for (const QString &keyword : highlighter.keywords()) {
        QTest::newRow(qPrintable(keyword)) << keyword;
    }
}

void TestSyntaxHighlighter::everyKeywordIsHighlighted()
{
    QFETCH(const QString, keyword);

    const QString text = QString("x %1 y").arg(keyword);
    const int start = text.indexOf(keyword);
    const Highlighted highlighted(text);

    QVERIFY(highlighted.spanHasColor(start, keyword.size(),
                                     highlighted.rules().keywordColor()));
    QVERIFY(highlighted.colorAt(start - 1) != highlighted.rules().keywordColor());
    QVERIFY(highlighted.colorAt(start + keyword.size())
            != highlighted.rules().keywordColor());
}

void TestSyntaxHighlighter::keywordInsideIdentifierIsNotHighlighted_data()
{
    everyKeywordIsHighlighted_data();
}

void TestSyntaxHighlighter::keywordInsideIdentifierIsNotHighlighted()
{
    QFETCH(const QString, keyword);

    const QString text = QString("x_%1_y").arg(keyword);
    const Highlighted highlighted(text);

    for (int position = 0; position < text.size(); ++position) {
        QVERIFY(highlighted.colorAt(position) != highlighted.rules().keywordColor());
    }
}

void TestSyntaxHighlighter::functionCallIsHighlighted()
{
    const QString name = "compute";
    const QString text = QString("x = %1(y);").arg(name);
    const int start = text.indexOf(name);
    const Highlighted highlighted(text);

    QVERIFY(highlighted.spanHasColor(start, name.size(),
                                     highlighted.rules().functionColor()));
    QVERIFY(highlighted.colorAt(text.indexOf('y'))
            != highlighted.rules().functionColor());
}

void TestSyntaxHighlighter::lineCommentCoversRestOfLine()
{
    const QString code = "x = y;";
    const QString text = code + " // note";
    const int start = text.indexOf("//");
    const Highlighted highlighted(text);

    QVERIFY(highlighted.spanHasColor(start, text.size() - start,
                                     highlighted.rules().commentColor()));
    QVERIFY(!highlighted.spanHasColor(0, code.size(), highlighted.rules().commentColor()));
}

void TestSyntaxHighlighter::stringLiteralIsHighlighted()
{
    const QString literal = "\"hello world\"";
    const QString text = QString("s = %1;").arg(literal);
    const int start = text.indexOf(literal);
    const Highlighted highlighted(text);

    QVERIFY(highlighted.spanHasColor(start, literal.size(),
                                     highlighted.rules().stringColor()));
    QVERIFY(highlighted.colorAt(start - 1) != highlighted.rules().stringColor());
    QVERIFY(highlighted.colorAt(start + literal.size())
            != highlighted.rules().stringColor());
}

void TestSyntaxHighlighter::blockCommentSpansLinesAndEnds()
{
    const QString comment = "/* first\n middle\n last */";
    const QString text = comment + " y";
    const Highlighted highlighted(text);

    for (int position = 0; position < comment.size(); ++position) {
        if (comment.at(position) != '\n') {
            QVERIFY2(highlighted.colorAt(position) == highlighted.rules().commentColor(),
                     qPrintable(QString("position %1 is not a comment").arg(position)));
        }
    }
    QVERIFY(highlighted.colorAt(text.indexOf('y', comment.size()))
            != highlighted.rules().commentColor());
}

QTEST_MAIN(TestSyntaxHighlighter)
#include "test_SyntaxHighlighter.moc"
