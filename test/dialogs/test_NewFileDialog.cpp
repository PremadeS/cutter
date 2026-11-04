/**
 * @file test_NewFileDialog.cpp
 * @brief The file selection step of opening a binary.
 *
 * NewFileDialog validates the chosen file and hands it to InitialOptionsDialog;
 * cancelling there brings a fresh NewFileDialog back. Real bugs lived in this loop:
 * a second file rejected after cancelling, and an error that kept appearing after a
 * valid file was chosen.
 */

#include "Fixtures.h"
#include "TestEnv.h"
#include "UiHelpers.h"

#include "dialogs/InitialOptionsDialog.h"
#include "dialogs/NewFileDialog.h"

#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QTest>

namespace {

bool submitFile(NewFileDialog *dialog, const QString &path)
{
    return CutterTest::setLineEditText(dialog, "newFileEdit", path)
            && CutterTest::clickButton(dialog, "loadFileButton");
}

} // namespace

class TestNewFileDialog : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void reselectAfterCancelingInitialOptions();
    void errorClearedAfterValidSelection();

private:
    QString helloPath;
    QString crackmePath;
};

void TestNewFileDialog::initTestCase()
{
    CUTTER_REQUIRE_FIXTURE(helloPath, CutterTest::Fixtures::helloX64());
    CUTTER_REQUIRE_FIXTURE(crackmePath, CutterTest::Fixtures::crackmeX86());
}

void TestNewFileDialog::init()
{
    QTRY_VERIFY(CutterTest::findVisibleTopLevel<NewFileDialog>());
}

void TestNewFileDialog::cleanup()
{
    if (auto *optionsDialog = CutterTest::findVisibleTopLevel<InitialOptionsDialog>()) {
        CutterTest::clickButton(optionsDialog, "cancelButton");
    }
}

void TestNewFileDialog::reselectAfterCancelingInitialOptions()
{
    CutterTest::MessageBoxWatcher messageBoxes;

    QPointer<NewFileDialog> newFileDialog = CutterTest::findVisibleTopLevel<NewFileDialog>();
    QVERIFY(submitFile(newFileDialog, helloPath));

    QPointer<InitialOptionsDialog> optionsDialog;
    QTRY_VERIFY((optionsDialog = CutterTest::findVisibleTopLevel<InitialOptionsDialog>()));
    QTRY_VERIFY(newFileDialog.isNull() || !newFileDialog->isVisible());

    QVERIFY(CutterTest::clickButton(optionsDialog, "cancelButton"));

    QTRY_VERIFY(optionsDialog.isNull() || !optionsDialog->isVisible());
    QTRY_VERIFY((newFileDialog = CutterTest::findVisibleTopLevel<NewFileDialog>()));
    QVERIFY(submitFile(newFileDialog, crackmePath));

    QTRY_VERIFY(CutterTest::findVisibleTopLevel<InitialOptionsDialog>());
    QCOMPARE(messageBoxes.seenTexts(), QStringList());
}

void TestNewFileDialog::errorClearedAfterValidSelection()
{
    const QString missingPath = QDir(QDir::tempPath()).filePath("cutter-test-missing-binary");
    QVERIFY(!QFileInfo::exists(missingPath));

    CutterTest::MessageBoxWatcher messageBoxes;

    QPointer<NewFileDialog> newFileDialog = CutterTest::findVisibleTopLevel<NewFileDialog>();
    QVERIFY(submitFile(newFileDialog, missingPath));
    QTRY_COMPARE(messageBoxes.seenTexts().size(), 1);
    QVERIFY(!newFileDialog.isNull() && newFileDialog->isVisible());
    QVERIFY(!CutterTest::findVisibleTopLevel<InitialOptionsDialog>());

    QVERIFY(submitFile(newFileDialog, helloPath));
    QTRY_VERIFY(CutterTest::findVisibleTopLevel<InitialOptionsDialog>());
    QCOMPARE(messageBoxes.seenTexts().size(), 1);
}

CUTTER_APP_TEST_MAIN(TestNewFileDialog)
#include "test_NewFileDialog.moc"
