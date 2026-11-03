/**
 * @file test_ListFilters.cpp
 * @brief The Strings dock's filters do what the filter UI shows, also after a refresh.
 *
 * The section combo and the text filter are wired to the proxy model by the dock, and a
 * refresh rebuilds both the model and the combo. A broken connection, or a refresh that
 * resets one side but not the other, leaves the list filtered differently from what the
 * filter bar says.
 */

#include "Fixtures.h"
#include "TestEnv.h"
#include "ViewHelpers.h"

#include "widgets/ComboQuickFilterView.h"
#include "widgets/StringsWidget.h"

#include <QComboBox>
#include <QHash>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSortFilterProxyModel>
#include <QTest>
#include <QTreeView>

namespace {

constexpr int stringsTimeoutMs = 30000;
constexpr int needleLength = 4;

StringDescription stringAt(const QModelIndex &index)
{
    return index.data(StringsModel::stringDescriptionRole).value<StringDescription>();
}

bool isPlainNeedle(const QString &text)
{
    for (const QChar c : text) {
        if (!c.isLetterOrNumber()) {
            return false;
        }
    }
    return !text.isEmpty();
}

} // namespace

class TestListFilters : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void sectionComboShowsOnlyThatSection();
    void filtersMatchFilterBarAfterRefresh();

private:
    QString pickNeedle() const;
    int expectedRows(const QString &section) const;

    StringsWidget *strings = nullptr;
    QSortFilterProxyModel *proxy = nullptr;
    QAbstractItemModel *source = nullptr;
    AbstractFilterView *filterView = nullptr;
    QComboBox *sections = nullptr;
};

void TestListFilters::initTestCase()
{
    MainWindow *main = CutterTest::mainWindow();
    QVERIFY(main);
    strings = main->findChild<StringsWidget *>();
    QVERIFY(strings);
    strings->toggleDockWidget(true);
    strings->raiseMemoryWidget();

    auto *view = strings->findChild<QTreeView *>("stringsTreeView");
    QVERIFY(view);
    proxy = qobject_cast<QSortFilterProxyModel *>(view->model());
    QVERIFY(proxy);
    source = proxy->sourceModel();
    auto *comboFilter = strings->findChild<ComboQuickFilterView *>();
    QVERIFY(comboFilter);
    filterView = comboFilter;
    sections = comboFilter->comboBox();

    QVERIFY2(CutterTest::waitForRows(source, stringsTimeoutMs), "the Strings dock stayed empty");
}

void TestListFilters::cleanup()
{
    filterView->lineEdit()->clear();
    sections->setCurrentIndex(0);
    QTRY_COMPARE(proxy->rowCount(), source->rowCount());
}

QString TestListFilters::pickNeedle() const
{
    for (int row = 0; row < source->rowCount(); ++row) {
        const QString candidate = stringAt(source->index(row, 0)).string.left(needleLength);
        if (candidate.size() == needleLength && isPlainNeedle(candidate)) {
            return candidate;
        }
    }
    return {};
}

int TestListFilters::expectedRows(const QString &section) const
{
    int count = 0;
    for (int row = 0; row < source->rowCount(); ++row) {
        const StringDescription description = stringAt(source->index(row, 0));
        if ((section.isEmpty() || description.section == section)
            && description.string.contains(proxy->filterRegularExpression())) {
            ++count;
        }
    }
    return count;
}

void TestListFilters::sectionComboShowsOnlyThatSection()
{
    QHash<QString, int> perSection;
    for (int row = 0; row < source->rowCount(); ++row) {
        ++perSection[stringAt(source->index(row, 0)).section];
    }

    QString section;
    for (auto it = perSection.cbegin(); it != perSection.cend(); ++it) {
        if (it.value() < source->rowCount() && sections->findData(it.key()) > 0) {
            section = it.key();
            break;
        }
    }
    if (section.isEmpty()) {
        QSKIP("all strings are in one section, nothing to filter out");
    }

    sections->setCurrentIndex(sections->findData(section));

    QTRY_COMPARE(proxy->rowCount(), perSection.value(section));
    for (int row = 0; row < proxy->rowCount(); ++row) {
        QCOMPARE(stringAt(proxy->index(row, 0)).section, section);
    }
}

void TestListFilters::filtersMatchFilterBarAfterRefresh()
{
    const QString needle = pickNeedle();
    if (needle.isEmpty()) {
        QSKIP("no string to build a filter from");
    }

    int sectionIndex = -1;
    for (int row = 0; row < source->rowCount() && sectionIndex <= 0; ++row) {
        const StringDescription description = stringAt(source->index(row, 0));
        if (description.string.contains(needle)) {
            sectionIndex = sections->findData(description.section);
        }
    }
    if (sectionIndex > 0) {
        sections->setCurrentIndex(sectionIndex);
    }
    filterView->lineEdit()->setText(needle);
    QTRY_COMPARE(proxy->rowCount(), expectedRows(sections->currentData().toString()));
    QVERIFY(proxy->rowCount() > 0);

    QSignalSpy reset(source, &QAbstractItemModel::modelReset);
    CutterTest::core()->triggerRefreshAll();
    QTRY_VERIFY_WITH_TIMEOUT(reset.count() > 0, stringsTimeoutMs);
    QTRY_VERIFY_WITH_TIMEOUT(!CutterTest::core()->getAsyncTaskManager()->getTasksRunning(),
                             stringsTimeoutMs);

    QCOMPARE(filterView->lineEdit()->text(), needle);
    const QString shownSection = sections->currentData().toString();
    QCOMPARE(proxy->rowCount(), expectedRows(shownSection));
    for (int row = 0; row < proxy->rowCount(); ++row) {
        const StringDescription description = stringAt(proxy->index(row, 0));
        QVERIFY(description.string.contains(proxy->filterRegularExpression()));
        if (!shownSection.isEmpty()) {
            QCOMPARE(description.section, shownSection);
        }
    }
}

CUTTER_APP_TEST_MAIN_WITH_FILE(TestListFilters, CutterTest::Fixtures::helloX64())
#include "test_ListFilters.moc"
