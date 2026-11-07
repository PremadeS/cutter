#include "common/Helpers.h"

#include <QTest>
#include <QSignalSpy>
#include <QPushButton>
#include <QComboBox>
#include <QTreeWidget>

class TestHelpers : public QObject
{
    Q_OBJECT

private slots:
    void testFormatByteCount();
    void sizesAreShownWithoutENotation_data();
    void sizesAreShownWithoutENotation();
    void testSetCheckedWithoutSignals();
    void testSelectIndexByData();
    void testApplyColorToSvg();
    void testAppendRow();
};

void TestHelpers::testFormatByteCount()
{
    QCOMPARE(qhelpers::formatByteCount(0), QString("0"));
    QCOMPARE(qhelpers::formatByteCount(512), QString("512 B"));
    QCOMPARE(qhelpers::formatByteCount(1024), QString("1 kB"));
    QCOMPARE(qhelpers::formatByteCount(1048576), QString("1 MB"));
    QCOMPARE(qhelpers::formatByteCount(1073741824), QString("1 GB"));
}

void TestHelpers::sizesAreShownWithoutENotation_data()
{
    constexpr ut64 kilobyte = 1024;
    constexpr ut64 megabyte = kilobyte * kilobyte;
    QTest::addColumn<ut64>("bytes");
    QTest::addColumn<QString>("shown");

    QTest::addRow("1000 bytes") << ut64(1000) << QString("1000 B");
    QTest::addRow("1023 bytes") << ut64(1023) << QString("1023 B");
    QTest::addRow("one and a half kB") << ut64(1536) << QString("1.5 kB");
    QTest::addRow("1006 kB") << ut64(1030000) << QString("1006 kB");
    QTest::addRow("one byte under a MB") << megabyte - 1 << QString("1 MB");
    QTest::addRow("ten MB") << 10 * megabyte << QString("10 MB");
}

void TestHelpers::sizesAreShownWithoutENotation()
{
    QFETCH(const ut64, bytes);
    QFETCH(const QString, shown);

    QCOMPARE(qhelpers::formatByteCount(bytes), shown);
}

void TestHelpers::testSetCheckedWithoutSignals()
{
    QPushButton button("Test");
    button.setCheckable(true);

    QSignalSpy spy(&button, &QPushButton::toggled);

    // Verify signal is NOT emitted when using qhelpers::setCheckedWithoutSignals
    qhelpers::setCheckedWithoutSignals(&button, true);
    QCOMPARE(spy.count(), 0);
    QVERIFY(button.isChecked());
}

void TestHelpers::testSelectIndexByData()
{
    QComboBox combo;
    combo.addItem("Item 0", 100);
    combo.addItem("Item 1", 200);
    combo.addItem("Item 2", 300);

    // Select existing data
    qhelpers::selectIndexByData(&combo, 200, 0);
    QCOMPARE(combo.currentIndex(), 1);

    // Select non-existing data, should fallback to defaultIndex
    qhelpers::selectIndexByData(&combo, 999, 2);
    QCOMPARE(combo.currentIndex(), 2);
}

void TestHelpers::testApplyColorToSvg()
{
    const QByteArray originalSvg = R"(<svg><path fill="#000000" style="fill:#123456;"/></svg>)";
    const QColor newColor(0xFF, 0x00, 0x00); // Red (#ff0000)

    const QByteArray coloredSvg = qhelpers::applyColorToSvg(originalSvg, newColor);

    QVERIFY(!coloredSvg.contains("#000000"));
    QVERIFY(!coloredSvg.contains("#123456"));
    QVERIFY(coloredSvg.contains("#ff0000"));
}


void TestHelpers::testAppendRow()
{
    QTreeWidget treeWidget;
    treeWidget.setColumnCount(6);

    QTreeWidgetItem *item = qhelpers::appendRow(&treeWidget, "Col1", "Col2", "Col3");

    QVERIFY(item != nullptr);
    QCOMPARE(treeWidget.topLevelItemCount(), 1);
    QCOMPARE(item->text(0), QString("0")); // dummy hidden column
    QCOMPARE(item->text(1), QString("Col1"));
    QCOMPARE(item->text(2), QString("Col2"));
    QCOMPARE(item->text(3), QString("Col3"));
}


QTEST_MAIN(TestHelpers)
#include "test_Helpers.moc"
