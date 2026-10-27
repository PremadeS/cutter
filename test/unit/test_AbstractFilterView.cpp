/**
 * @file test_AbstractFilterView.cpp
 * @brief Debounce, clear, close and show behavior of the list quick filter.
 *
 * Every list dock filters through this view. The debounce must coalesce typing into
 * one update, or large lists re-filter on every keystroke; clear and close must
 * reset the filter; showing it must focus the input.
 */

#include "widgets/AbstractFilterView.h"
#include "widgets/ItemCountLineEdit.h"

#include <QLineEdit>
#include <QSignalSpy>
#include <QtTest/QtTest>

namespace {

constexpr int debounceWaitMs = 300;

} // namespace

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
    void testRapidTypingEmitsOnce();
    void testClearFilter();
    void testCloseFilter();
    void testShowFilter();
};

void TestAbstractFilterView::testFilterTextChanged_data() // NOLINT
{
    QTest::addColumn<QString>("inputText");
    QTest::addColumn<QString>("expectedEmittedText");

    QTest::newRow("simple_function_search") << "main" << "main";
    QTest::newRow("empty_string_search") << "" << "";
}

void TestAbstractFilterView::testFilterTextChanged()
{
    QFETCH(QString, inputText);
    QFETCH(QString, expectedEmittedText);

    TestableFilterView filterView;
    filterView.lineEdit()->setText("seed_text");

    QSignalSpy spy(&filterView, &AbstractFilterView::filterTextChanged);

    filterView.lineEdit()->setText(inputText);

    QCOMPARE(spy.count(), 0);

    QVERIFY(spy.wait(debounceWaitMs));

    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), expectedEmittedText);
}

void TestAbstractFilterView::testRapidTypingEmitsOnce()
{
    TestableFilterView filterView;
    QSignalSpy spy(&filterView, &AbstractFilterView::filterTextChanged);

    QTest::keyClicks(filterView.lineEdit(), "main");
    QCOMPARE(filterView.lineEdit()->text(), QString("main"));
    QCOMPARE(spy.count(), 0);

    QVERIFY(spy.wait(debounceWaitMs));
    QTest::qWait(debounceWaitMs);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toString(), QString("main"));
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
    QCOMPARE(filterView.focusWidget(), static_cast<QWidget *>(filterView.lineEdit()));
}

QTEST_MAIN(TestAbstractFilterView)
#include "test_AbstractFilterView.moc"