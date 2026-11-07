/**
 * @file test_NewFileDialog.cpp
 * @brief The New File dialog's recent lists and the Open button.
 *
 * Each recent file and project shows its name and the folder it is in, and removing one must
 * also remove it from the saved list, or it comes back the next time Cutter starts. Opening
 * needs a file: only "Don't open any file" opens Cutter without one.
 */

#include "TestEnv.h"
#include "UiHelpers.h"

#include "common/Configuration.h"
#include "core/MainWindow.h"
#include "dialogs/InitialOptionsDialog.h"
#include "dialogs/NewFileDialog.h"

#include <QAction>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QListWidget>
#include <QTemporaryDir>
#include <QTest>

namespace {

QListWidgetItem *itemForPath(const QListWidget *list, const QString &path)
{
    for (int row = 0; row < list->count(); ++row) {
        QListWidgetItem *item = list->item(row);
        if (item->data(Qt::UserRole).toStringList().value(1) == path) {
            return item;
        }
    }
    return nullptr;
}

bool createFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly);
}

QStringList savedProjectPaths()
{
    QStringList paths;
    for (const RecentFileEntry &project : Config()->getRecentProjects()) {
        paths << project.path;
    }
    return paths;
}

} // namespace

class TestNewFileDialog : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();

    /** @brief A recent file shows its own folder, even when its name also appears in the path. */
    void recentFileShowsItsFolder();
    /** @brief A recent project removed from the list is not saved anymore. */
    void removedRecentProjectStaysRemoved();
    /** @brief Open with an empty path asks for a file instead of opening nothing. */
    void openWithoutAFileAsksForOne();
};

void TestNewFileDialog::cleanup()
{
    if (auto *optionsDialog = CutterTest::findVisibleTopLevel<InitialOptionsDialog>()) {
        CutterTest::clickButton(optionsDialog, "cancelButton");
    }
}

void TestNewFileDialog::recentFileShowsItsFolder()
{
    const QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString name = QStringLiteral("s");
    QVERIFY(QDir(dir.path()).mkdir(name));
    const QString folder = QDir::toNativeSeparators(QDir(dir.path()).filePath(name));
    const QString path = QDir::toNativeSeparators(QDir(folder).filePath(name));
    QVERIFY(createFile(path));
    Config()->setRecentFiles({ { QStringLiteral("file://"), path } });

    const NewFileDialog dialog(CutterTest::mainWindow());
    const auto *recents = dialog.findChild<QListWidget *>("recentsListWidget");
    QVERIFY(recents);
    const QListWidgetItem *item = itemForPath(recents, path);
    QVERIFY(item);

    const QString home = QDir::toNativeSeparators(QDir::homePath());
    QString shownFolder = folder;
    if (shownFolder.startsWith(home + QDir::separator())) {
        shownFolder.replace(0, home.size(), QStringLiteral("~"));
    }
    QCOMPARE(item->text().section(QLatin1Char('\n'), 1, 1), shownFolder);
}

void TestNewFileDialog::removedRecentProjectStaysRemoved()
{
    const QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = QDir::toNativeSeparators(QDir(dir.path()).filePath("project.rzdb"));
    QVERIFY(createFile(path));
    Config()->addRecentProject(path);

    NewFileDialog dialog(CutterTest::mainWindow());
    auto *projects = dialog.findChild<QListWidget *>("projectsListWidget");
    auto *remove = dialog.findChild<QAction *>("actionRemoveProject");
    QVERIFY(projects && remove);
    QListWidgetItem *item = itemForPath(projects, path);
    QVERIFY(item);

    projects->setCurrentItem(item);
    remove->trigger();

    QVERIFY(!savedProjectPaths().contains(path));
}

void TestNewFileDialog::openWithoutAFileAsksForOne()
{
    CutterTest::MessageBoxWatcher messageBoxes;
    NewFileDialog dialog(CutterTest::mainWindow());
    const auto *fileless = dialog.findChild<QCheckBox *>("checkBoxFilelessOpen");
    QVERIFY(fileless && !fileless->isChecked());

    QVERIFY(CutterTest::setLineEditText(&dialog, "newFileEdit", QString()));
    QVERIFY(CutterTest::clickButton(&dialog, "loadFileButton"));

    QCOMPARE(messageBoxes.seenTexts().size(), 1);
    QVERIFY(!CutterTest::findVisibleTopLevel<InitialOptionsDialog>());
}

CUTTER_APP_TEST_MAIN(TestNewFileDialog)
#include "test_NewFileDialog.moc"
