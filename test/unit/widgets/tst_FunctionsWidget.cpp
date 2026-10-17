#include "CutterTest.h"
#include "helper/ProxyModelTestHelper.h"
#include "widgets/FunctionsWidget.h"

#include <QFont>
#include <QSignalSpy>
#include <QtTest/QtTest>

class TestFunctionsWidget : public CutterTest
{
    Q_OBJECT

public:
    TestFunctionsWidget() : CutterTest(false, false) {}

private slots:
    void testInitialModelCounts();
    void testDataDisplayRoles();
    void testSearchFiltering();
    void testSortingByOffsetAndName();
    void testFunctionRenameSignal();
};

void TestFunctionsWidget::testInitialModelCounts()
{
    const QFont font("Monospace", 10);
    FunctionModel model(false, font, font);

    FunctionDescription f1;
    f1.name = "main";
    f1.offset = 0x401000;
    f1.linearSize = 64;
    FunctionDescription f2;
    f2.name = "sym.imp.printf";
    f2.offset = 0x402000;
    f2.linearSize = 16;
    model.setFunctions({ f1, f2 });

    QCOMPARE(model.rowCount(QModelIndex()), 2);
    QCOMPARE(model.columnCount(QModelIndex()), static_cast<int>(FunctionModel::ColumnCount));

    model.setNested(true);
    QCOMPARE(model.columnCount(QModelIndex()), 1);
}

void TestFunctionsWidget::testDataDisplayRoles()
{
    const QFont font("Monospace", 10);
    FunctionModel model(false, font, font);

    FunctionDescription f1;
    f1.name = "entry0";
    f1.offset = 0x400000;
    f1.linearSize = 32;
    model.setFunctions({ f1 });

    const QModelIndex nameIdx = model.index(0, FunctionModel::NameColumn);
    const QModelIndex sizeIdx = model.index(0, FunctionModel::SizeColumn);

    QCOMPARE(model.data(nameIdx, Qt::DisplayRole).toString(), QString("entry0"));
    QCOMPARE(model.data(sizeIdx, Qt::DisplayRole).toString(), QString("32"));

    const QVariant funcVar = model.data(nameIdx, FunctionModel::functionDescriptionRole);
    QVERIFY(funcVar.canConvert<FunctionDescription>());
    QCOMPARE(funcVar.value<FunctionDescription>().offset, static_cast<RVA>(0x400000));
}

void TestFunctionsWidget::testSearchFiltering()
{
    const QFont font("Monospace", 10);
    auto *model = new FunctionModel(false, font, font, this);

    FunctionDescription f1;
    f1.name = "sym.main";
    f1.offset = 0x1000;
    FunctionDescription f2;
    f2.name = "sym.alloc";
    f2.offset = 0x2000;
    FunctionDescription f3;
    f3.name = "sym.write";
    f3.offset = 0x3000;

    model->setFunctions({ f1, f2, f3 });
    FunctionSortFilterProxyModel proxy(model, this);

    ProxyModelTestHelper::assertFilter(&proxy, "", 3);
    ProxyModelTestHelper::assertFilter(&proxy, "alloc", 1, "sym.alloc");
    ProxyModelTestHelper::assertFilter(&proxy, "", 3);
}

void TestFunctionsWidget::testSortingByOffsetAndName()
{
    const QFont font("Monospace", 10);
    auto *model = new FunctionModel(false, font, font, this);

    FunctionDescription f1;
    f1.name = "beta";
    f1.offset = 0x3000;
    FunctionDescription f2;
    f2.name = "alpha";
    f2.offset = 0x1000;

    model->setFunctions({ f1, f2 });
    FunctionSortFilterProxyModel proxy(model, this);

    ProxyModelTestHelper::assertSortSequence(&proxy, FunctionModel::NameColumn, Qt::AscendingOrder,
                                             { "alpha", "beta" });

    ProxyModelTestHelper::assertSortSequence(&proxy, FunctionModel::OffsetColumn,
                                             Qt::DescendingOrder, { "beta", "alpha" });
}

void TestFunctionsWidget::testFunctionRenameSignal()
{
    const QFont font("Monospace", 10);
    FunctionModel model(false, font, font);

    FunctionDescription f1;
    f1.name = "old_name";
    f1.offset = 0x401000;
    model.setFunctions({ f1 });

    const QSignalSpy spy(&model, &FunctionModel::dataChanged);

    QMetaObject::invokeMethod(&model, "functionRenamed", Q_ARG(RVA, 0x401000),
                              Q_ARG(QString, QString("new_name")));

    QCOMPARE(model.getFunctions().at(0).name, QString("new_name"));
    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(TestFunctionsWidget)
#include "tst_FunctionsWidget.moc"