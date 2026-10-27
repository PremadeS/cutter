/**
 * @file test_InitialOptionsDialog.cpp
 * @brief The options step between choosing a file and starting analysis.
 *
 * Every choice the user makes here (architecture, bits, endianness, analysis level)
 * must end up in the InitialOptions that analysis runs with, and cancelling must
 * return to file selection without starting anything. A dropped option means Cutter
 * silently analyzes the binary differently from what the user asked for.
 */

#include "CutterApplication.h"
#include "Fixtures.h"
#include "TestEnv.h"
#include "UiHelpers.h"
#include "common/AsyncTask.h"
#include "core/MainWindow.h"
#include "dialogs/InitialOptionsDialog.h"
#include "dialogs/NewFileDialog.h"

#include <QComboBox>
#include <QDir>
#include <QPointer>
#include <QSlider>
#include <QTest>

namespace {

constexpr int analysisTimeoutMs = 60000;
constexpr int autoAnalysisLevel = 1;

bool submitFile(const QString &path)
{
    auto *newFileDialog = CutterTest::findVisibleTopLevel<NewFileDialog>();
    return newFileDialog && CutterTest::setLineEditText(newFileDialog, "newFileEdit", path)
            && CutterTest::clickButton(newFileDialog, "loadFileButton");
}

} // namespace

class TestInitialOptionsDialog : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void cancelReturnsToNewFileDialog();
    void chosenOptionsReachAnalysis();

private:
    QString path;
};

void TestInitialOptionsDialog::initTestCase()
{
    path = CutterTest::fixturePath(CutterTest::Fixtures::helloX64());
    if (path.isEmpty()) {
        QSKIP("rizin-testbins not available, set CUTTER_TESTBINS_DIR");
    }
}

void TestInitialOptionsDialog::init()
{
    QTRY_VERIFY(CutterTest::findVisibleTopLevel<NewFileDialog>());
}

void TestInitialOptionsDialog::cancelReturnsToNewFileDialog()
{
    QVERIFY(submitFile(path));
    QPointer<InitialOptionsDialog> optionsDialog;
    QTRY_VERIFY((optionsDialog = CutterTest::findVisibleTopLevel<InitialOptionsDialog>()));

    QVERIFY(CutterTest::clickButton(optionsDialog, "cancelButton"));

    QTRY_VERIFY(optionsDialog.isNull() || !optionsDialog->isVisible());
    QTRY_VERIFY(CutterTest::findVisibleTopLevel<NewFileDialog>());
    QVERIFY(!CutterTest::core()->getAsyncTaskManager()->getTasksRunning());
    QVERIFY(!CutterTest::mainWindow()->isVisible());
}

void TestInitialOptionsDialog::chosenOptionsReachAnalysis()
{
    QVERIFY(submitFile(path));
    InitialOptionsDialog *optionsDialog = nullptr;
    QTRY_VERIFY((optionsDialog = CutterTest::findVisibleTopLevel<InitialOptionsDialog>()));

    auto *arch = optionsDialog->findChild<QComboBox *>("archComboBox");
    auto *bits = optionsDialog->findChild<QComboBox *>("bitsComboBox");
    auto *endianness = optionsDialog->findChild<QComboBox *>("endiannessComboBox");
    auto *level = optionsDialog->findChild<QSlider *>("analysisSlider");
    QVERIFY(arch && bits && endianness && level);

    const int x86 = arch->findData("x86");
    const int bits32 = bits->findText("32");
    QVERIFY(x86 >= 0);
    QVERIFY(bits32 >= 0);
    arch->setCurrentIndex(x86);
    bits->setCurrentIndex(bits32);
    endianness->setCurrentIndex(1);
    level->setValue(autoAnalysisLevel);

    QVERIFY(CutterTest::clickButton(optionsDialog, "okButton"));

    auto *app = qobject_cast<CutterApplication *>(QCoreApplication::instance());
    QVERIFY(app);
    const InitialOptions options = app->getInitialOptions();
    QVERIFY(options.filename.endsWith(QDir::toNativeSeparators(path)));
    QCOMPARE(options.arch, QString("x86"));
    QCOMPARE(options.bits, 32);
    QCOMPARE(options.endian, InitialOptions::Endianness::Little);
    QCOMPARE(options.analysisCmd.size(), 1);
    QCOMPARE(options.analysisCmd.first().command, QString("aaa"));

    QTRY_VERIFY_WITH_TIMEOUT(CutterTest::mainWindow()->isVisible(), analysisTimeoutMs);
}

CUTTER_APP_TEST_MAIN(TestInitialOptionsDialog)
#include "test_InitialOptionsDialog.moc"
