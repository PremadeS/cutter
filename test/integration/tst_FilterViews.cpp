#include "CutterApplication.h"
#include "core/Cutter.h"
#include "core/MainWindow.h"
#include "widgets/AbstractFilterView.h"
#include "widgets/ComboQuickFilterView.h"
#include "widgets/QuickFilterView.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDockWidget>
#include <QLineEdit>
#include <QSignalSpy>
#include <QSortFilterProxyModel>
#include <QTest>

class TestFilterViews : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testFilterViewsInDocks_data();
    void testFilterViewsInDocks();
    void cleanupTestCase();

private:
    MainWindow *mainWindow = nullptr;
};

void TestFilterViews::initTestCase()
{
    Q_INIT_RESOURCE(resources);

    for (QWidget *widget : QApplication::topLevelWidgets()) {
        if (auto *mw = qobject_cast<MainWindow *>(widget)) {
            mainWindow = mw;
            break;
        }
    }

    QVERIFY2(mainWindow != nullptr, "MainWindow not found among top-level widgets!");

    mainWindow->show();
    QTest::qWait(100);

    qInstallMessageHandler(nullptr);
}

void TestFilterViews::testFilterViewsInDocks_data()
{
    QTest::addColumn<QString>("dockObjectName");

    const auto docks = mainWindow->findChildren<QDockWidget *>();
    int discoveredCount = 0;

    for (auto *dock : docks) {
        if (dock->findChild<AbstractFilterView *>()) {
            const QString objectName = dock->objectName();
            if (!objectName.isEmpty()) {
                QTest::newRow(objectName.toUtf8().constData()) << objectName;
                discoveredCount++;
            }
        }
    }

    QVERIFY2(discoveredCount > 0, "Failed to discover any dock widgets with AbstractFilterView!");
}

void TestFilterViews::testFilterViewsInDocks()
{
    QFETCH(QString, dockObjectName);

    auto *dock = mainWindow->findChild<QDockWidget *>(dockObjectName);
    QVERIFY2(dock != nullptr, "Dock widget missing from layout");

    dock->show();
    dock->raise();
    QApplication::processEvents();

    auto *comboFilter = dock->findChild<ComboQuickFilterView *>();
    auto *filter = comboFilter ? static_cast<AbstractFilterView *>(comboFilter)
                               : dock->findChild<AbstractFilterView *>();

    QVERIFY2(filter != nullptr, "Dock widget missing AbstractFilterView child");

    auto *itemView = dock->findChild<QAbstractItemView *>();
    QVERIFY2(itemView != nullptr, "Dock widget missing QAbstractItemView child");

    auto *proxyModel = qobject_cast<QSortFilterProxyModel *>(itemView->model());
    QVERIFY2(proxyModel != nullptr, "ItemView model is not a QSortFilterProxyModel");

    QTest::qWait(100);

    const int initialRows = proxyModel->rowCount();

    itemView->setFocus();
    QApplication::processEvents();

    QTest::keyClick(itemView, Qt::Key_F, Qt::ControlModifier);
    QApplication::processEvents();

    QVERIFY2(filter->lineEdit()->hasFocus(),
             "Filter shortcut failed to focus QLineEdit on target widget");

    QTest::keyClicks(filter->lineEdit(), "a");
    QApplication::processEvents();

    QCOMPARE(filter->lineEdit()->text(), QString("a"));
    QVERIFY(proxyModel->rowCount() >= 0);

    if (comboFilter) {
        QComboBox *combo = comboFilter->comboBox();
        if (combo->count() > 0) {
            for (int i = 0; i < combo->count(); ++i) {
                combo->setCurrentIndex(i);
                QApplication::processEvents();
                QVERIFY(proxyModel->rowCount() >= 0);
            }
            combo->setCurrentIndex(0);
        }
    }

    QTest::keyClick(filter->lineEdit(), Qt::Key_Escape);
    QApplication::processEvents();

    if (!filter->lineEdit()->text().isEmpty()) {
        filter->clearFilter();
        QApplication::processEvents();
    }

    QCOMPARE(proxyModel->rowCount(), initialRows);

    if (filter->isVisible()) {
        const QSignalSpy spyClosed(filter, &AbstractFilterView::filterClosed);
        filter->closeFilter();
        QTest::qWait(50);

        QCOMPARE(spyClosed.count(), 1);
    }

    itemView->setFocus();
    QApplication::processEvents();
    QVERIFY(itemView->hasFocus());
}

void TestFilterViews::cleanupTestCase()
{
    mainWindow = nullptr;
}

int main(int argc, char *argv[])
{
    QString binaryPath = "/Users/premades/Documents/rizinorg/test-programs/strings";
    if (!QFile::exists(binaryPath)) {
        binaryPath = QFINDTESTDATA("ls");
        if (binaryPath.isEmpty()) {
            binaryPath = "/bin/ls";
        }
    }
    QByteArray binaryPathBa = binaryPath.toUtf8();

    char appName[] = "tst_FilterViews";
    char analFlag[] = "-A";
    char analVal[] = "1";
    char noRedirect[] = "--no-output-redirect";
    char *cutterArgv[] = { appName, analFlag, analVal, noRedirect, binaryPathBa.data(), nullptr };
    int cutterArgc = 5;

    const CutterApplication app(cutterArgc, cutterArgv);
    qInstallMessageHandler(nullptr);

    TestFilterViews test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_FilterViews.moc"