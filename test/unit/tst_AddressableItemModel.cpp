#include "common/AddressableItemModel.h"

#include <QStandardItemModel>
#include <QtTest/QtTest>

class TestableAddressableModel : public AddressableItemModel<QStandardItemModel>
{
public:
    explicit TestableAddressableModel(QObject *parent = nullptr)
        : AddressableItemModel<QStandardItemModel>(parent)
    {
    }

    RVA address(const QModelIndex &index) const override
    {
        if (!index.isValid()) {
            return UT64_MAX;
        }
        return static_cast<RVA>(index.row() * 0x00001000 + 0x00400000);
    }

    QString name(const QModelIndex &index) const override
    {
        if (!index.isValid()) {
            return QString();
        }
        return QString("symbol_at_row_%1").arg(index.row());
    }
};

class testAddressableItemModel : public QObject
{
    Q_OBJECT

private slots:
    void testFilterProxyModel_data();
    void testFilterProxyModel();
};

void testAddressableItemModel::testFilterProxyModel_data()
{
    QTest::addColumn<QString>("filterPattern");
    QTest::addColumn<int>("expectedProxyRowCount");
    QTest::addColumn<RVA>("expectedFirstRowAddress");
    QTest::addColumn<QString>("expectedFirstRowName");

    QTest::newRow("match_all_rows")
            << "symbol" << 3 << static_cast<RVA>(0x00400000) << "symbol_at_row_0";
    QTest::newRow("filter_row_2_only")
            << "row_2" << 1 << static_cast<RVA>(0x00402000) << "symbol_at_row_2";
    QTest::newRow("filter_no_matches") << "nonexistent_pattern" << 0 << UT64_MAX << "";
}

void testAddressableItemModel::testFilterProxyModel()
{
    const QFETCH(QString, filterPattern);
    const QFETCH(int, expectedProxyRowCount);
    const QFETCH(RVA, expectedFirstRowAddress);
    const QFETCH(QString, expectedFirstRowName);

    TestableAddressableModel sourceModel;
    sourceModel.setRowCount(3);
    sourceModel.setColumnCount(1);

    for (int row = 0; row < 3; ++row) {
        QModelIndex idx = sourceModel.index(row, 0);
        sourceModel.setData(idx, sourceModel.name(idx), Qt::DisplayRole);
    }

    AddressableFilterProxyModel proxyModel(&sourceModel, nullptr);
    proxyModel.setFilterFixedString(filterPattern);

    QCOMPARE(proxyModel.rowCount(), expectedProxyRowCount);

    if (expectedProxyRowCount > 0) {
        QModelIndex proxyIdx = proxyModel.index(0, 0);
        QCOMPARE(proxyModel.address(proxyIdx), expectedFirstRowAddress);
        QCOMPARE(proxyModel.name(proxyIdx), expectedFirstRowName);
    } else {
        QModelIndex invalidIdx;
        QCOMPARE(proxyModel.address(invalidIdx), UT64_MAX);
    }
}

QTEST_MAIN(testAddressableItemModel)
#include "tst_AddressableItemModel.moc"