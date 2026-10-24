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

#include <functional>

using FilterEvaluator = std::function<int(const QString &searchQuery, const QString &comboData)>;

Q_DECLARE_METATYPE(FilterEvaluator)

class TestFilterViews : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testFilterViewsInDocks_data(); // NOLINT
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

void TestFilterViews::testFilterViewsInDocks_data() // NOLINT
{
    QTest::addColumn<QString>("dockObjectName");
    QTest::addColumn<QString>("searchQuery");
    QTest::addColumn<FilterEvaluator>("evaluator");

    const FilterEvaluator stringsEval = [](const QString &query, const QString &comboData) -> int {
        const bool raw = Config()->getShowRawStrings();
        const auto strings = Core()->getAllStrings(raw);
        int count = 0;
        for (const auto &s : strings) {
            const bool textMatches =
                    query.isEmpty() || s.string.contains(query, Qt::CaseInsensitive);
            const bool sectionMatches = comboData.isEmpty() || (s.section == comboData);

            if (textMatches && sectionMatches) {
                count++;
            }
        }
        return count;
    };
    QTest::newRow("StringsWidget") << "StringsWidget" << "color" << stringsEval;

    const FilterEvaluator functionsEval = [](const QString &query, const QString &) -> int {
        const auto funcs = Core()->getAllFunctions();
        int count = 0;
        for (const auto &f : funcs) {
            if (query.isEmpty() || f.name.contains(query, Qt::CaseInsensitive)) {
                count++;
            }
        }
        return count;
    };
    QTest::newRow("FunctionsWidget") << "FunctionsWidget" << "main" << functionsEval;
}

void TestFilterViews::testFilterViewsInDocks()
{
    const QFETCH(QString, dockObjectName);
    const QFETCH(QString, searchQuery);
    const QFETCH(FilterEvaluator, evaluator);

    auto *dock = mainWindow->findChild<QDockWidget *>(dockObjectName);
    if (!dock) {
        QSKIP("Specified dock widget not present in MainWindow layout");
    }

    dock->show();
    dock->raise();
    QApplication::processEvents();

    auto *comboFilter = dock->findChild<ComboQuickFilterView *>();
    auto *filter = comboFilter ? static_cast<AbstractFilterView *>(comboFilter)
                               : dock->findChild<AbstractFilterView *>();

    if (!filter) {
        QSKIP("Dock widget does not contain an AbstractFilterView child");
    }

    filter->showFilter();
    QVERIFY(filter->isVisible());

    auto *itemView = dock->findChild<QAbstractItemView *>();
    QVERIFY2(itemView != nullptr, "Dock widget missing QAbstractItemView child");

    auto *proxyModel = qobject_cast<QSortFilterProxyModel *>(itemView->model());
    QVERIFY2(proxyModel != nullptr, "ItemView model is not a QSortFilterProxyModel");

    QTRY_VERIFY2(proxyModel->rowCount() > 0, "Proxy model rows failed to populate in time");

    const int queryMatchCount = evaluator(searchQuery, QString());
    QVERIFY2(queryMatchCount > 0,
             "Failed to find or sample a valid non-empty search query from the loaded binary!");

    if (comboFilter) {
        QComboBox *combo = comboFilter->comboBox();
        QTRY_VERIFY2(combo->count() > 0, "Combo box items failed to populate in time");

        for (int i = 0; i < combo->count(); ++i) {
            combo->setCurrentIndex(i);
            const QString comboData = combo->currentData().toString();
            const QString comboText = combo->itemText(i);

            filter->clearFilter();
            QApplication::processEvents();

            const int expectedComboOnlyCount = evaluator(QString(), comboData);
            QTRY_COMPARE(proxyModel->rowCount(), expectedComboOnlyCount);

            filter->lineEdit()->setText(searchQuery);
            QApplication::processEvents();

            const int expectedCombinedCount = evaluator(searchQuery, comboData);

            QTRY_COMPARE(proxyModel->rowCount(), expectedCombinedCount);
        }
        combo->setCurrentIndex(0);
    }

    filter->lineEdit()->setText(searchQuery);
    QApplication::processEvents();

    const int expectedCount = evaluator(searchQuery, QString());
    QTRY_COMPARE(proxyModel->rowCount(), expectedCount);

    filter->clearFilter();
    QApplication::processEvents();

    const int resetExpectedCount = evaluator(QString(), QString());
    QTRY_COMPARE(proxyModel->rowCount(), resetExpectedCount);

    const QSignalSpy spyClosed(filter, &AbstractFilterView::filterClosed);
    filter->closeFilter();
    QTest::qWait(50);

    QVERIFY(!filter->isVisible());
    QCOMPARE(spyClosed.count(), 1);
}

void TestFilterViews::cleanupTestCase()
{
    mainWindow = nullptr;
}

int main(int argc, char *argv[])
{
    // Resolve binary path cleanly without passing raw absolute paths into QFINDTESTDATA
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