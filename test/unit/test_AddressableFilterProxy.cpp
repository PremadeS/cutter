/**
 * @file test_AddressableFilterProxy.cpp
 * @brief Address and name lookup through a sorted, filtered list proxy.
 *
 * Every list dock sorts and filters through AddressableFilterProxyModel, and
 * activating or right-clicking a row uses the proxy's address() and name(). If the
 * mapping to the source row breaks, the user seeks to, renames or comments the
 * wrong item.
 */

#include "common/AddressableItemModel.h"

#include <QStandardItemModel>
#include <QtTest/QtTest>

namespace {

constexpr int addressRole = Qt::UserRole;

class AddressListModel : public AddressableItemModel<QStandardItemModel>
{
public:
    void addItem(const QString &name, RVA address)
    {
        auto *item = new QStandardItem(name);
        item->setData(QVariant::fromValue<qulonglong>(address), addressRole);
        appendRow(item);
    }

    RVA address(const QModelIndex &index) const override
    {
        return index.isValid() ? data(index, addressRole).toULongLong() : RVA_INVALID;
    }

    QString name(const QModelIndex &index) const override
    {
        return index.isValid() ? data(index).toString() : QString();
    }
};

struct Item
{
    QString name;
    RVA address;
};

const QList<Item> items = {
    { "main", 0x1040 }, { "entry0", 0x1000 }, { "sym.helper", 0x1100 }, { "fcn.00001200", 0x1200 }
};

RVA addressOf(const QString &name)
{
    for (const Item &item : items) {
        if (item.name == name) {
            return item.address;
        }
    }
    return RVA_INVALID;
}

} // namespace

class TestAddressableFilterProxy : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void addressAndNameFollowSorting();
    void addressAndNameFollowFiltering();

private:
    AddressListModel *source = nullptr;
    AddressableFilterProxyModel *proxy = nullptr;
};

void TestAddressableFilterProxy::init()
{
    source = new AddressListModel;
    for (const Item &item : items) {
        source->addItem(item.name, item.address);
    }
    proxy = new AddressableFilterProxyModel(source, nullptr);
}

void TestAddressableFilterProxy::cleanup()
{
    delete proxy;
    delete source;
    proxy = nullptr;
    source = nullptr;
}

void TestAddressableFilterProxy::addressAndNameFollowSorting()
{
    proxy->sort(0, Qt::AscendingOrder);

    QCOMPARE(proxy->rowCount(), int(items.size()));
    QCOMPARE(proxy->name(proxy->index(0, 0)), QString("entry0"));
    for (int row = 0; row < proxy->rowCount(); ++row) {
        const QModelIndex index = proxy->index(row, 0);
        const QString name = proxy->name(index);
        QCOMPARE(name, proxy->data(index).toString());
        QCOMPARE(proxy->address(index), addressOf(name));
    }
}

void TestAddressableFilterProxy::addressAndNameFollowFiltering()
{
    proxy->sort(0, Qt::DescendingOrder);
    proxy->setFilterFixedString("e");

    QCOMPARE(proxy->rowCount(), 2);
    for (int row = 0; row < proxy->rowCount(); ++row) {
        const QModelIndex index = proxy->index(row, 0);
        const QString name = proxy->name(index);
        QVERIFY(name.contains('e'));
        QCOMPARE(proxy->address(index), addressOf(name));
    }
    QCOMPARE(proxy->address(QModelIndex()), RVA_INVALID);
}

QTEST_MAIN(TestAddressableFilterProxy)
#include "test_AddressableFilterProxy.moc"
