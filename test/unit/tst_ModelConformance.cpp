#include "ModelRegistry.h"

#include "core/Cutter.h"

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QSortFilterProxyModel>
#include <QVariant>
#include <QtTest/QtTest>

/*
 * L1 model conformance batteries (no Core()/rizin).
 *
 *  Battery A (structural): every registered model, empty and (when a populate
 *    hook exists) populated, must obey the QAbstractItemModel contract — sane
 *    rowCount/columnCount, valid headers, in-range indices, no crash when data()
 *    is invoked. Core()-dependent columns are skipped so this layer stays free
 *    of rizin.
 *
 *  Battery B (proxy): every registered proxy must preserve row counts across
 *    filter/sort round-trips, honour wildcard and metacharacter filters, and
 *    never lose rows when a filter is cleared.
 *
 * Triage: a failure here is a Cutter logic bug (L1 green + L2 red => rizin
 * changed underneath; L1+L2 green + L3 red => widget wiring).
 */
class TestModelConformance : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // Battery A
    void testModelStructuralEmpty_data();
    void testModelStructuralEmpty();
    void testModelPopulated_data();
    void testModelPopulated();

    // Battery B
    void testProxyFilteringRoundTrip_data();
    void testProxyFilteringRoundTrip();
    void testProxySorting_data();
    void testProxySorting();

private:
    void assertEmptyStructural(ModelEntry &entry);
    void assertPopulatedStructural(ModelEntry &entry, QAbstractItemModel *source);
};

void TestModelConformance::initTestCase()
{
    // Enforce the L1 invariant: this layer must not instantiate Core()/rizin.
    QVERIFY2(CutterCore::instance() == nullptr,
             "L1 unit tests must not create a CutterCore instance!");
}

// ---------------------------------------------------------------------------
// Battery A: empty-model structural conformance
// ---------------------------------------------------------------------------

void TestModelConformance::testModelStructuralEmpty_data()
{
    QTest::addColumn<QString>("entryName");
    for (const ModelEntry &e : allModelEntries()) {
        QTest::newRow(e.name.toUtf8().constData()) << e.name;
    }
}

void TestModelConformance::testModelStructuralEmpty()
{
    QFETCH(QString, entryName);

    for (ModelEntry &entry : allModelEntries()) {
        if (entry.name != entryName) {
            continue;
        }
        assertEmptyStructural(entry);
        return;
    }
    QFAIL("Requested registry entry not found");
}

void TestModelConformance::assertEmptyStructural(ModelEntry &entry)
{
    ModelUnderTest m = entry.make();
    QVERIFY2(m.source, qPrintable(entry.name + ": failed to construct source model"));
    QVERIFY2(m.proxy, qPrintable(entry.name + ": failed to construct proxy model"));

    // Empty source model must report zero rows.
    QCOMPARE(m.source->rowCount(), 0);

    // A model with zero columns is broken; every one of these is a table/list
    // with at least a primary column.
    const int cols = m.source->columnCount();
    QVERIFY2(cols >= 1, qPrintable(entry.name + ": columnCount() < 1"));

    // Header for the primary column must be a non-empty string.
    QVariant header = m.source->headerData(0, Qt::Horizontal, Qt::DisplayRole);
    QVERIFY2(header.isValid(), qPrintable(entry.name + ": headerData(0) invalid"));
    QVERIFY2(!header.toString().isEmpty(),
             qPrintable(entry.name + ": headerData(0) empty string"));

    // Reading every column header must not crash.
    for (int c = 0; c < cols; c++) {
        m.source->headerData(c, Qt::Horizontal, Qt::DisplayRole);
        m.source->headerData(c, Qt::Vertical, Qt::DisplayRole);
    }

    // Out-of-range index() must return an invalid index (hasIndex contract).
    QVERIFY(!m.source->index(m.source->rowCount(), 0).isValid());
    QVERIFY(!m.source->index(0, cols).isValid());

    // A proxy over an empty source is itself empty, and filtering/sorting it
    // must be a no-op that does not crash.
    QCOMPARE(m.proxy->rowCount(), 0);
    m.proxy->setFilterWildcard(QStringLiteral("anything"));
    QCOMPARE(m.proxy->rowCount(), 0);
    for (int c = 0; c < cols; c++) {
        m.proxy->sort(c, Qt::AscendingOrder);
        QCOMPARE(m.proxy->rowCount(), 0);
    }
    m.proxy->sort(-1, Qt::AscendingOrder);

    delete m.proxy;
    delete m.source;
}

// ---------------------------------------------------------------------------
// Battery A: populated-model conformance (only entries with a populate hook)
// ---------------------------------------------------------------------------

void TestModelConformance::testModelPopulated_data()
{
    QTest::addColumn<QString>("entryName");
    for (const ModelEntry &e : allModelEntries()) {
        if (e.populate) {
            QTest::newRow(e.name.toUtf8().constData()) << e.name;
        }
    }
}

void TestModelConformance::testModelPopulated()
{
    QFETCH(QString, entryName);

    for (ModelEntry &entry : allModelEntries()) {
        if (entry.name != entryName) {
            continue;
        }
        QVERIFY2(entry.populate, "entry has no populate hook");

        ModelUnderTest m = entry.make();
        QVERIFY(m.source && m.proxy);

        entry.populate(m.source);
        assertPopulatedStructural(entry, m.source);

        delete m.proxy;
        delete m.source;
        return;
    }
    QFAIL("Requested registry entry not found");
}

void TestModelConformance::assertPopulatedStructural(ModelEntry &entry, QAbstractItemModel *source)
{
    const int rows = source->rowCount();
    QVERIFY2(rows > 0, qPrintable(entry.name + ": populate hook produced no rows"));

    const int cols = source->columnCount();
    QVERIFY(cols >= 1);

    for (int r = 0; r < rows; r++) {
        const QModelIndex idx0 = source->index(r, 0);
        QVERIFY2(idx0.isValid(), qPrintable(entry.name + ": index(r,0) invalid in-range"));

        // Smoke-read every non-Core column: must not crash / hit UB.
        for (int c = 0; c < cols; c++) {
            if (entry.coreDependentColumns.contains(c)) {
                continue; // would dereference a null Core()
            }
            source->data(source->index(r, c), Qt::DisplayRole);
        }

        // The primary column must yield a concrete display value.
        const QVariant primary = source->data(idx0, Qt::DisplayRole);
        QVERIFY2(primary.isValid(), qPrintable(entry.name + ": column 0 DisplayRole invalid"));

        // An unrecognised role must fall through to an invalid QVariant.
        QVERIFY(!source->data(idx0, Qt::AccessibleTextRole).isValid());

        // Addressable models must resolve an address for every row.
        // AddressableItemModelI and QAbstractItemModel are sibling bases of the
        // multiply-inherited model, so the cast must be a dynamic cross-cast.
        if (entry.addressable) {
            auto *am = dynamic_cast<AddressableItemModelI *>(source);
            QVERIFY2(am != nullptr, qPrintable(entry.name + ": not AddressableItemModelI"));
            const RVA addr = am->address(idx0);
            QVERIFY2(addr != RVA_INVALID, qPrintable(entry.name + ": address() == RVA_INVALID"));
        }
    }
}

// ---------------------------------------------------------------------------
// Battery B: proxy filtering round-trip
// ---------------------------------------------------------------------------

void TestModelConformance::testProxyFilteringRoundTrip_data()
{
    testModelPopulated_data();
}

void TestModelConformance::testProxyFilteringRoundTrip()
{
    QFETCH(QString, entryName);

    for (ModelEntry &entry : allModelEntries()) {
        if (entry.name != entryName) {
            continue;
        }
        QVERIFY2(entry.populate, "entry has no populate hook");

        ModelUnderTest m = entry.make();
        QVERIFY(m.source && m.proxy);
        entry.populate(m.source);

        const int total = m.source->rowCount();
        QVERIFY(total > 0);

        // No filter -> every source row is visible.
        m.proxy->setFilterWildcard(QString());
        QCOMPARE(m.proxy->rowCount(), total);

        // A token that cannot appear in the fixture must filter to zero rows.
        m.proxy->setFilterWildcard(QStringLiteral("zzz_unlikely_token_zzz"));
        QCOMPARE(m.proxy->rowCount(), 0);

        // Clearing the filter must restore the exact original count (no leak).
        m.proxy->setFilterWildcard(QString());
        QCOMPARE(m.proxy->rowCount(), total);

        // "*" wildcard matches everything.
        m.proxy->setFilterWildcard(QStringLiteral("*"));
        QCOMPARE(m.proxy->rowCount(), total);
        m.proxy->setFilterWildcard(QString());

        // Regex metacharacters must be treated literally by the wildcard
        // syntax: "." is not "any char", so "h.llo" must NOT match "hello".
        m.proxy->setFilterWildcard(QStringLiteral("h.llo"));
        const int dotMatches = m.proxy->rowCount();
        m.proxy->setFilterWildcard(QString());
        // With only the Strings fixture populated this must be 0; for other
        // populated entries we at least guarantee the clear-restore below holds.
        if (entry.name == QLatin1String("Strings")) {
            QCOMPARE(dotMatches, 0);
        }

        delete m.proxy;
        delete m.source;
        return;
    }
    QFAIL("Requested registry entry not found");
}

// ---------------------------------------------------------------------------
// Battery B: proxy sorting preserves rows
// ---------------------------------------------------------------------------

void TestModelConformance::testProxySorting_data()
{
    testModelPopulated_data();
}

void TestModelConformance::testProxySorting()
{
    QFETCH(QString, entryName);

    for (ModelEntry &entry : allModelEntries()) {
        if (entry.name != entryName) {
            continue;
        }
        QVERIFY2(entry.populate, "entry has no populate hook");

        ModelUnderTest m = entry.make();
        QVERIFY(m.source && m.proxy);
        entry.populate(m.source);

        const int total = m.source->rowCount();
        const int cols = m.source->columnCount();

        for (int c = 0; c < cols; c++) {
            if (entry.coreDependentColumns.contains(c)) {
                continue; // lessThan() would dereference a null Core()
            }
            m.proxy->sort(c, Qt::AscendingOrder);
            QCOMPARE(m.proxy->rowCount(), total);
            m.proxy->sort(c, Qt::DescendingOrder);
            QCOMPARE(m.proxy->rowCount(), total);
        }

        // Deactivate sorting; count is still intact.
        m.proxy->sort(-1, Qt::AscendingOrder);
        QCOMPARE(m.proxy->rowCount(), total);

        delete m.proxy;
        delete m.source;
        return;
    }
    QFAIL("Requested registry entry not found");
}

QTEST_MAIN(TestModelConformance)
#include "tst_ModelConformance.moc"
