#include "common/Helpers.h"

#include <QTest>
#include <QSignalSpy>
#include <QPushButton>
#include <QComboBox>
#include <QTreeWidget>
#include <QMenu>
#include <QAction>
#include <QStandardItemModel>
#include <QListView>

class TestHelpers : public QObject
{
    Q_OBJECT

private slots:
    void testFormatByteCount();
    void testSetCheckedWithoutSignals();
    void testSelectIndexByData();
    void testApplyColorToSvg();
    void testPrependQAction();
    void testAppendRow();
    void testSelectFirstItem();
};

void TestHelpers::testFormatByteCount()
{
    QCOMPARE(qhelpers::formatByteCount(0), QString("0"));
    QCOMPARE(qhelpers::formatByteCount(512), QString("512  B")); // Two spaces for raw bytes
    QCOMPARE(qhelpers::formatByteCount(1024), QString("1 kB"));
    QCOMPARE(qhelpers::formatByteCount(1048576), QString("1 MB"));
    QCOMPARE(qhelpers::formatByteCount(1073741824), QString("1 GB"));
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

    // Verify signal IS emitted when using standard setChecked
    button.setChecked(false);
    QCOMPARE(spy.count(), 1);
    QVERIFY(!button.isChecked());
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

void TestHelpers::testPrependQAction()
{
    QMenu menu;
    auto *action1 = new QAction("First", &menu);
    auto *action2 = new QAction("Prepended", &menu);

    menu.addAction(action1);
    qhelpers::prependQAction(action2, &menu);

    const auto actions = menu.actions();
    QCOMPARE(actions.size(), 2);
    QCOMPARE(actions.first(), action2);
    QCOMPARE(actions.last(), action1);
}

void TestHelpers::testAppendRow()
{
    QTreeWidget treeWidget;
    treeWidget.setColumnCount(6);

    QTreeWidgetItem *item = qhelpers::appendRow(&treeWidget, "Col1", "Col2", "Col3");

    QVERIFY(item != nullptr);
    QCOMPARE(treeWidget.topLevelItemCount(), 1);
    QCOMPARE(item->text(0), QString("0")); // Dummy hidden column
    QCOMPARE(item->text(1), QString("Col1"));
    QCOMPARE(item->text(2), QString("Col2"));
    QCOMPARE(item->text(3), QString("Col3"));
}

void TestHelpers::testSelectFirstItem()
{
    QListView listView;
    QStandardItemModel model(2, 1);
    model.setItem(0, 0, new QStandardItem("Row 0"));
    model.setItem(1, 0, new QStandardItem("Row 1"));
    listView.setModel(&model);

    QVERIFY(qhelpers::selectFirstItem(&listView));
    QCOMPARE(listView.currentIndex().row(), 0);

    // Test nullptr or empty model handling
    QListView emptyView;
    QVERIFY(!qhelpers::selectFirstItem(&emptyView));
}

QTEST_MAIN(TestHelpers)
#include "tst_Helpers.moc"
