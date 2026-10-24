#include "ModelRegistry.h"

#include "core/Cutter.h"
#include "widgets/StringsWidget.h"

#include <QModelIndex>
#include <QVariant>
#include <QtTest/QtTest>

/*
 * L1 strings-filter semantics (no Core()/rizin).
 *
 * This is the direct regression guard for the reported bug: the strings dock
 * offers a per-section combo (fed by Core()->getSectionList()) and a wildcard
 * quick-filter. StringsProxyModel::filterAcceptsRow requires BOTH
 *     selectedSection == str.section  AND  filterStringContains(str.string)
 * when a section is selected, and only the text match when "(all)" is chosen.
 *
 * We drive the proxy with synthetic StringDescription rows via the
 * StringsModel::setStrings() seam and assert the exact intersection semantics,
 * including empty sections, case-insensitivity, and regex metacharacters.
 */
class TestStringsFilter : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void testNoFilterShowsAll();
    void testSectionFilterExactMatch();
    void testAllSectionClearsFilter();
    void testSectionAndWildcardIntersection();
    void testEmptySectionOnlyVisibleUnderAll();
    void testWildcardIsCaseInsensitive();
    void testRegexMetacharactersAreLiteral();
    void testWildcardAsteriskMatchesEverything();
    void testSortingPreservesSectionFilter();

private:
    StringsModel *m_model = nullptr;
    StringsProxyModel *m_proxy = nullptr;

    void populate();
    int countForSection(const QString &section);
};

void TestStringsFilter::initTestCase()
{
    // L1 must stay free of Core()/rizin.
    QVERIFY2(CutterCore::instance() == nullptr,
             "L1 unit tests must not create a CutterCore instance!");

    m_model = new StringsModel(this);
    m_proxy = new StringsProxyModel(m_model, this);
    populate();
}

void TestStringsFilter::init()
{
    // Re-establish the canonical fixture and a clean filter/section/sort state
    // before EVERY test. This keeps tests independent even if an earlier test
    // aborts mid-way (a failing QCOMPARE skips its own restore step).
    populate();
    m_proxy->setFilterWildcard(QString());
    m_proxy->setSelectedSection(QString());
    m_proxy->sort(-1, Qt::AscendingOrder);
}

void TestStringsFilter::cleanup()
{
    // Reset filter state between rows so tests are independent.
    if (m_proxy) {
        m_proxy->setFilterWildcard(QString());
        m_proxy->setSelectedSection(QString());
        m_proxy->sort(-1, Qt::AscendingOrder);
    }
}

void TestStringsFilter::populate()
{
    QList<StringDescription> strings;
    strings << makeTestString(0x1000, "hello world", "latin1", ".text", 11, 11);
    strings << makeTestString(0x1010, "cutter rocks", "utf8", ".rodata", 12, 12);
    strings << makeTestString(0x1020, "another string", "latin1", ".text", 14, 14);
    strings << makeTestString(0x1030, "final entry", "utf8", ".data", 11, 11);
    // A string that lives outside any named section.
    strings << makeTestString(0x1040, "orphan bytes", "latin1", "", 12, 12);
    m_model->setStrings(strings);
}

int TestStringsFilter::countForSection(const QString &section)
{
    m_proxy->setSelectedSection(section);
    return m_proxy->rowCount();
}

void TestStringsFilter::testNoFilterShowsAll()
{
    m_proxy->setSelectedSection(QString());
    m_proxy->setFilterWildcard(QString());
    QCOMPARE(m_proxy->rowCount(), m_model->rowCount());
    QCOMPARE(m_proxy->rowCount(), 5);
}

void TestStringsFilter::testSectionFilterExactMatch()
{
    // Exact-match on section name: ".text" selects the two .text rows only.
    QCOMPARE(countForSection(".text"), 2);
    QCOMPARE(countForSection(".rodata"), 1);
    QCOMPARE(countForSection(".data"), 1);
    // No section is named ".bss" -> zero rows.
    QCOMPARE(countForSection(".bss"), 0);
}

void TestStringsFilter::testAllSectionClearsFilter()
{
    // "(all)" in the UI maps to an empty selectedSection (the combo's first
    // item carries no data). An empty selection must show every row.
    QCOMPARE(countForSection(QString()), 5);
}

void TestStringsFilter::testSectionAndWildcardIntersection()
{
    // Section ".text" + text "another" -> the single .text row matching.
    m_proxy->setSelectedSection(".text");
    m_proxy->setFilterWildcard("another");
    QCOMPARE(m_proxy->rowCount(), 1);
    const QModelIndex idx = m_proxy->index(0, StringsModel::StringColumn);
    QCOMPARE(m_proxy->data(idx, Qt::DisplayRole).toString(), QString("another string"));

    // Section ".rodata" + text "hello" -> no intersection (hello is .text).
    m_proxy->setSelectedSection(".rodata");
    m_proxy->setFilterWildcard("hello");
    QCOMPARE(m_proxy->rowCount(), 0);

    // Clearing the text while keeping the section restores the section rows.
    m_proxy->setFilterWildcard(QString());
    QCOMPARE(m_proxy->rowCount(), 1);
}

void TestStringsFilter::testEmptySectionOnlyVisibleUnderAll()
{
    // The orphan string has an empty section. Selecting any real section must
    // exclude it; selecting "(all)" (empty section) must include it.
    m_proxy->setFilterWildcard("orphan");
    QCOMPARE(countForSection(QString()), 1);
    QCOMPARE(countForSection(".text"), 0);
    QCOMPARE(countForSection(".rodata"), 0);
    QCOMPARE(countForSection(".data"), 0);
    m_proxy->setFilterWildcard(QString());
}

void TestStringsFilter::testWildcardIsCaseInsensitive()
{
    // StringsProxyModel sets Qt::CaseInsensitive in its constructor.
    m_proxy->setFilterWildcard("HELLO");
    QCOMPARE(m_proxy->rowCount(), 1);
    m_proxy->setFilterWildcard("CuTtEr");
    QCOMPARE(m_proxy->rowCount(), 1);
}

void TestStringsFilter::testRegexMetacharactersAreLiteral()
{
    // In wildcard syntax "." is a literal, not "any character". "h.llo" must
    // not match "hello world" — a regression here would mean the proxy is
    // treating the quick-filter as a raw regex.
    m_proxy->setFilterWildcard("h.llo");
    QCOMPARE(m_proxy->rowCount(), 0);

    // Parentheses in the pattern are literal too: match a string that
    // genuinely contains regex metacharacters in its content.
    QList<StringDescription> extra;
    extra << makeTestString(0x1050, "value=42 (test)", "latin1", ".text", 15, 15);
    m_model->setStrings([&]() {
        QList<StringDescription> all;
        all << makeTestString(0x1000, "hello world", "latin1", ".text", 11, 11);
        all << makeTestString(0x1010, "cutter rocks", "utf8", ".rodata", 12, 12);
        all << makeTestString(0x1020, "another string", "latin1", ".text", 14, 14);
        all << makeTestString(0x1030, "final entry", "utf8", ".data", 11, 11);
        all << makeTestString(0x1040, "orphan bytes", "latin1", "", 12, 12);
        all << makeTestString(0x1050, "value=42 (test)", "latin1", ".text", 15, 15);
        return all;
    }());

    m_proxy->setSelectedSection(QString());
    m_proxy->setFilterWildcard("value=42 (test)");
    QCOMPARE(m_proxy->rowCount(), 1);

    // An UNBALANCED paren must still match: in wildcard syntax "(" is escaped
    // to a literal, so "value=42 (test" remains a valid substring of the data
    // "value=42 (test)" -> 1 row. Were the pattern fed to the regex engine as
    // raw source, the unbalanced group would make it invalid and match NOTHING
    // (0 rows). So 1 here is the proof that parentheses are escaped, not
    // interpreted as regex groups.
    m_proxy->setFilterWildcard("value=42 (test");
    QCOMPARE(m_proxy->rowCount(), 1);

    // Restore the original fixture for subsequent tests.
    populate();
}

void TestStringsFilter::testWildcardAsteriskMatchesEverything()
{
    m_proxy->setSelectedSection(QString());
    m_proxy->setFilterWildcard("*");
    QCOMPARE(m_proxy->rowCount(), m_model->rowCount());
}

void TestStringsFilter::testSortingPreservesSectionFilter()
{
    // Sorting must not disturb an active section filter.
    m_proxy->setSelectedSection(".text");
    m_proxy->sort(StringsModel::OffsetColumn, Qt::AscendingOrder);
    QCOMPARE(m_proxy->rowCount(), 2);
    m_proxy->sort(StringsModel::StringColumn, Qt::DescendingOrder);
    QCOMPARE(m_proxy->rowCount(), 2);
}

QTEST_MAIN(TestStringsFilter)
#include "tst_StringsFilter.moc"
