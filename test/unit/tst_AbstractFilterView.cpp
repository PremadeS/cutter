#include "widgets/AbstractFilterView.h"
#include "widgets/ItemCountLineEdit.h"

#include <QLineEdit>
#include <QSignalSpy>
#include <QtTest/QtTest>

// Concrete implementation of AbstractFilterView for testing
class TestableFilterView : public AbstractFilterView
{
    Q_OBJECT

public:
    explicit TestableFilterView(QWidget *parent = nullptr)
        : AbstractFilterView(parent), m_lineEdit(new ItemCountLineEdit(this))
    {
        setupSharedConnections();
    }

    ItemCountLineEdit *lineEdit() const override { return m_lineEdit; }

private:
    ItemCountLineEdit *m_lineEdit;
};

class TestAbstractFilterView : public QObject
{
    Q_OBJECT

private slots:
    void testFilterTextChanged_data(); // NOLINT
    void testFilterTextChanged();
    void testClearFilter();
    void testCloseFilter();
    void testShowFilter();
};

void TestAbstractFilterView::testFilterTextChanged_data() // NOLINT
{
    QTest::addColumn<QString>("inputText");
    QTest::addColumn<QString>("expectedEmittedText");

    QTest::newRow("simple_function_search") << "main" << "main";
    QTest::newRow("address_hex_search") << "0x04001000" << "0x04001000";
    QTest::newRow("special_characters_search")
            << "sym.imp.__libc_start_main" << "sym.imp.__libc_start_main";
    QTest::newRow("empty_string_search") << "" << "";
}

void TestAbstractFilterView::testFilterTextChanged()
{
    QFETCH(QString, inputText);
    QFETCH(QString, expectedEmittedText);

    TestableFilterView filterView;
    // Seed with initial text so setting "" actually fires textChanged
    filterView.lineEdit()->setText("seed_text");

    QSignalSpy spy(&filterView, &AbstractFilterView::filterTextChanged);

    filterView.lineEdit()->setText(inputText);

    // Debounce timer (150ms) should not have fired immediately
    QCOMPARE(spy.count(), 0);

    // Wait for the 150ms debounce timer to fire
    QVERIFY(spy.wait(300));

    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), expectedEmittedText);
}

void TestAbstractFilterView::testClearFilter()
{
    TestableFilterView filterView;
    filterView.lineEdit()->setText("pre_existing_filter_query");

    filterView.clearFilter();

    QCOMPARE(filterView.lineEdit()->text(), QString(""));
}

void TestAbstractFilterView::testCloseFilter()
{
    TestableFilterView filterView;
    filterView.show();
    filterView.lineEdit()->setText("active_search_string");

    QSignalSpy spy(&filterView, &AbstractFilterView::filterClosed);

    filterView.closeFilter();

    QCOMPARE(filterView.lineEdit()->text(), QString(""));
    QCOMPARE(filterView.isVisible(), false);
    QCOMPARE(spy.count(), 1);
}

void TestAbstractFilterView::testShowFilter()
{
    TestableFilterView filterView;
    filterView.hide();

    filterView.showFilter();

    QCOMPARE(filterView.isVisible(), true);
    // Use focusWidget() to verify focus assignment in headless test environments
    QCOMPARE(filterView.focusWidget(), static_cast<QWidget *>(filterView.lineEdit()));
}

QTEST_MAIN(TestAbstractFilterView)
#include "tst_AbstractFilterView.moc"