#ifndef PROXY_MODEL_TEST_HELPER_H
#define PROXY_MODEL_TEST_HELPER_H

#include <QSortFilterProxyModel>
#include <QStringList>
#include <QtTest/QtTest>

namespace ProxyModelTestHelper {

inline void assertFilter(QSortFilterProxyModel *proxy, const QString &filterText,
                         int expectedRowCount, const QString &expectedTopMatch = QString(),
                         int checkColumn = 0)
{
    proxy->setFilterFixedString(filterText);
    QCOMPARE(proxy->rowCount(), expectedRowCount);

    if (expectedRowCount > 0 && !expectedTopMatch.isNull()) {
        const QModelIndex idx = proxy->index(0, checkColumn);
        QCOMPARE(proxy->data(idx, Qt::DisplayRole).toString(), expectedTopMatch);
    }
}

inline void assertSortSequence(QSortFilterProxyModel *proxy, int sortColumn, Qt::SortOrder order,
                               const QStringList &expectedSequence, int checkColumn = 0)
{
    proxy->sort(sortColumn, order);
    QCOMPARE(proxy->rowCount(), expectedSequence.size());

    for (int i = 0; i < expectedSequence.size(); ++i) {
        const QModelIndex idx = proxy->index(i, checkColumn);
        QCOMPARE(proxy->data(idx, Qt::DisplayRole).toString(), expectedSequence.at(i));
    }
}

} // namespace ProxyModelTestHelper

#endif // PROXY_MODEL_TEST_HELPER_H