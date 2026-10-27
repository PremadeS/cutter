#include "RichTextPainter.h"

#include <QTest>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextCharFormat>

class TestRichTextPainter : public QObject
{
    Q_OBJECT

private slots:
    void testHtmlRichText();
    void testHtmlRichTextEscaping();
    void testCroppedUnchanged();
    void testCroppedTruncated();
    void testCroppedWithIndicator();
    void testCroppedMultiTokenIndicator();
    void testFromTextDocument();
};

void TestRichTextPainter::testHtmlRichText()
{
    RichTextPainter::CustomRichText t1;
    t1.text = "mov";
    t1.flags = RichTextPainter::FlagColor;
    t1.textColor = QColor("#ff0000");

    RichTextPainter::CustomRichText t2;
    t2.text = "eax";
    t2.flags = RichTextPainter::FlagAll;
    t2.textColor = QColor("#00ff00");
    t2.textBackground = QColor("#0000ff");
    t2.highlight = true;

    RichTextPainter::List list = { t1, t2 };

    QString html, plain;
    RichTextPainter::htmlRichText(list, html, plain);

    QCOMPARE(plain, QString("moveax"));
    QVERIFY(html.contains("<span style=\"color:#ff0000\">mov</span>"));
    QVERIFY(html.contains("<span style=\"color:#00ff00; background-color:#0000ff\"><u>eax</u></span>"));
}

void TestRichTextPainter::testHtmlRichTextEscaping()
{
    RichTextPainter::CustomRichText item;
    item.text = "<script>&";
    item.flags = RichTextPainter::FlagNone;

    QString html, plain;
    RichTextPainter::htmlRichText(RichTextPainter::List{ item }, html, plain);

    QCOMPARE(plain, QString("<script>&"));
    QCOMPARE(html, QString("<span>&lt;script&gt;&amp;</span>"));
}

void TestRichTextPainter::testCroppedUnchanged()
{
    RichTextPainter::CustomRichText item;
    item.text = "hello";

    bool croppedOut = true;
    auto result = RichTextPainter::cropped({ item }, 10, "...", &croppedOut);

    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].text, QString("hello"));
    QVERIFY(!croppedOut);
}

void TestRichTextPainter::testCroppedTruncated()
{
    RichTextPainter::CustomRichText item;
    item.text = "0123456789";

    bool croppedOut = false;
    auto result = RichTextPainter::cropped({ item }, 5, "", &croppedOut);

    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].text, QString("01234"));
    QVERIFY(croppedOut);
}

void TestRichTextPainter::testCroppedWithIndicator()
{
    RichTextPainter::CustomRichText item;
    item.text = "abcdefghij"; // 10 chars

    bool croppedOut = false;
    // max 7 chars, indicator "..." (3 chars) -> "abcd..."
    auto result = RichTextPainter::cropped({ item }, 7, "...", &croppedOut);

    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].text, QString("abcd..."));
    QVERIFY(croppedOut);
}

void TestRichTextPainter::testCroppedMultiTokenIndicator()
{
    RichTextPainter::CustomRichText t1;
    t1.text = "abc"; // 3 chars
    RichTextPainter::CustomRichText t2;
    t2.text = "defghij"; // 7 chars

    bool croppedOut = false;
    // max 5 chars, total 10 chars. Truncates across tokens and inserts indicator
    auto result = RichTextPainter::cropped({ t1, t2 }, 5, "...", &croppedOut);

    QVERIFY(croppedOut);
    QString total;
    for (const auto &item : result) {
        total += item.text;
    }
    QCOMPARE(total, QString("ab..."));
}

void TestRichTextPainter::testFromTextDocument()
{
    QTextDocument doc;
    QTextCursor cursor(&doc);

    QTextCharFormat fmt;
    fmt.setForeground(QBrush(Qt::red));
    cursor.insertText("RedText", fmt);

    auto list = RichTextPainter::fromTextDocument(doc);

    QCOMPARE(list.size(), 1);
    QCOMPARE(list[0].text, QString("RedText"));
    QCOMPARE(list[0].flags, RichTextPainter::FlagColor);
    QCOMPARE(list[0].textColor, QColor(Qt::red));
}

QTEST_MAIN(TestRichTextPainter)
#include "test_RichTextPainter.moc"
