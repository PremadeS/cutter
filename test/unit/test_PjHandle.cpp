#include "common/PjHandle.h"

#include <QtTest/QtTest>

#include <cstdlib>

class TestPjHandle : public QObject
{
    Q_OBJECT

private slots:
    void testLifetimeAndDrain();
    void testMoveConstructor();
    void testMoveAssignment();
};

void TestPjHandle::testLifetimeAndDrain()
{
    PjHandle handle;
    QVERIFY(static_cast<bool>(handle));
    QVERIFY(handle.get() != nullptr);

    // Populate JSON data
    pj_o(handle.get());
    pj_ks(handle.get(), "key", "value");
    pj_end(handle.get());

    char *json = handle.drain();
    QVERIFY(json != nullptr);
    QCOMPARE(QString(json), QString("{\"key\":\"value\"}"));

    // Post-drain handle must be empty
    QVERIFY(!static_cast<bool>(handle));
    QCOMPARE(handle.get(), static_cast<PJ *>(nullptr));

    std::free(json);
}

void TestPjHandle::testMoveConstructor()
{
    PjHandle handle1;
    PJ *rawPj = handle1.get();
    QVERIFY(rawPj != nullptr);

    PjHandle handle2(std::move(handle1));

    QCOMPARE(handle2.get(), rawPj);
    QVERIFY(!static_cast<bool>(handle1));
    QCOMPARE(handle1.get(), static_cast<PJ *>(nullptr));
}

void TestPjHandle::testMoveAssignment()
{
    PjHandle handle1;
    PjHandle handle2;
    PJ *rawPj1 = handle1.get();

    handle2 = std::move(handle1);

    QCOMPARE(handle2.get(), rawPj1);
    QVERIFY(!static_cast<bool>(handle1));
}

QTEST_MAIN(TestPjHandle)
#include "test_PjHandle.moc"
