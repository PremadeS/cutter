/**
 * @file test_OpenFile.cpp
 * @brief Opening a binary: NewFileDialog, then InitialOptionsDialog, then analysis.
 *
 * NewFileDialog validates the chosen file and hands it to InitialOptionsDialog; cancelling
 * there brings a fresh NewFileDialog back without starting anything. Real bugs lived in this
 * loop: a second file rejected after cancelling, and an error that kept appearing after a
 * valid file was chosen. Every option chosen (architecture, bits, endianness, analysis
 * level, debuginfod) must reach the InitialOptions analysis runs with, or Cutter silently
 * analyzes the binary differently from what the user asked for. A `<file>.rz` script next to the
 * binary is offered before the options, and the options dialog shows the analysis level that
 * will actually run, with or without a script.
 */

#include "CutterApplication.h"
#include "Fixtures.h"
#include "TestEnv.h"
#include "UiHelpers.h"
#include "common/AsyncTask.h"
#include "core/MainWindow.h"
#include "dialogs/InitialOptionsDialog.h"
#include "dialogs/NewFileDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QSlider>
#include <QTemporaryDir>
#include <QTest>

namespace {

constexpr int analysisTimeoutMs = 60000;
constexpr int autoAnalysisLevel = 1;

bool submitFile(NewFileDialog *dialog, const QString &path)
{
    return dialog && CutterTest::setLineEditText(dialog, "newFileEdit", path)
            && CutterTest::clickButton(dialog, "loadFileButton");
}

} // namespace

class TestOpenFile : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void reselectAfterCancelingOptions();
    void errorClearedAfterValidSelection();
    /** @brief Opening a binary with a `<binary>.rz` next to it offers to load that script. */
    void scriptNextToTheFileIsOffered();
    /** @brief The analysis checkbox and slider show the level that will run, script or not. */
    void loadedOptionsShowTheAnalysisThatWillRun_data();
    void loadedOptionsShowTheAnalysisThatWillRun();
    void chosenOptionsReachAnalysis();

private:
    QString helloPath;
    QString crackmePath;
};

void TestOpenFile::initTestCase()
{
    CUTTER_REQUIRE_FIXTURE(helloPath, CutterTest::Fixtures::helloX64());
    CUTTER_REQUIRE_FIXTURE(crackmePath, CutterTest::Fixtures::crackmeX86());
}

void TestOpenFile::init()
{
    QTRY_VERIFY(CutterTest::findVisibleTopLevel<NewFileDialog>());
}

void TestOpenFile::cleanup()
{
    if (auto *optionsDialog = CutterTest::findVisibleTopLevel<InitialOptionsDialog>()) {
        CutterTest::clickButton(optionsDialog, "cancelButton");
    }
}

void TestOpenFile::reselectAfterCancelingOptions()
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
    QVERIFY(!CutterTest::core()->getAsyncTaskManager()->getTasksRunning());
    QVERIFY(!CutterTest::mainWindow()->isVisible());
    QVERIFY(submitFile(newFileDialog, crackmePath));

    QTRY_VERIFY(CutterTest::findVisibleTopLevel<InitialOptionsDialog>());
    QCOMPARE(messageBoxes.seenTexts(), QStringList());
}

void TestOpenFile::errorClearedAfterValidSelection()
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

void TestOpenFile::scriptNextToTheFileIsOffered()
{
    const QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString binary =
            QDir::toNativeSeparators(QDir(dir.path()).filePath(QFileInfo(helloPath).fileName()));
    QVERIFY(QFile::copy(helloPath, binary));
    const QString script = binary + QStringLiteral(".rz");
    QFile scriptFile(script);
    QVERIFY(scriptFile.open(QIODevice::WriteOnly));
    scriptFile.close();

    CutterTest::MessageBoxWatcher messageBoxes;
    QVERIFY(submitFile(CutterTest::findVisibleTopLevel<NewFileDialog>(), binary));

    QTRY_VERIFY(CutterTest::findVisibleTopLevel<InitialOptionsDialog>());
    QCOMPARE(messageBoxes.seenTexts().size(), 1);
    QVERIFY(messageBoxes.seenTexts().first().contains(script));
}

void TestOpenFile::loadedOptionsShowTheAnalysisThatWillRun_data()
{
    QTest::addColumn<bool>("withScript");
    QTest::addColumn<bool>("autoAnalysis");
    QTest::addColumn<int>("expectedLevel");

    QTest::newRow("script and auto analysis") << true << true << autoAnalysisLevel;
    QTest::newRow("script without analysis") << true << false << 0;
    QTest::newRow("no script, no analysis") << false << false << 0;
}

void TestOpenFile::loadedOptionsShowTheAnalysisThatWillRun()
{
    QFETCH(const bool, withScript);
    QFETCH(const bool, autoAnalysis);
    QFETCH(const int, expectedLevel);

    InitialOptions options {};
    if (withScript) {
        options.script = QStringLiteral("script.rz");
    }
    if (!autoAnalysis) {
        options.analysisCmd = {};
    }

    InitialOptionsDialog dialog(CutterTest::mainWindow());
    dialog.loadOptions(options);

    const auto *level = dialog.findChild<QSlider *>("analysisSlider");
    const auto *enabled = dialog.findChild<QCheckBox *>("analysisCheckBox");
    QVERIFY(level && enabled);
    QCOMPARE(level->value(), expectedLevel);
    QCOMPARE(enabled->isChecked(), expectedLevel > 0);
}

void TestOpenFile::chosenOptionsReachAnalysis()
{
    const QString debuginfod = QStringLiteral("bin.dbginfo.debuginfod");
    CutterTest::core()->setConfig(debuginfod, true);

    QVERIFY(submitFile(CutterTest::findVisibleTopLevel<NewFileDialog>(), helloPath));
    InitialOptionsDialog *optionsDialog = nullptr;
    QTRY_VERIFY((optionsDialog = CutterTest::findVisibleTopLevel<InitialOptionsDialog>()));

    auto *arch = optionsDialog->findChild<QComboBox *>("archComboBox");
    auto *bits = optionsDialog->findChild<QComboBox *>("bitsComboBox");
    auto *endianness = optionsDialog->findChild<QComboBox *>("endiannessComboBox");
    auto *level = optionsDialog->findChild<QSlider *>("analysisSlider");
    auto *debuginfodBox = optionsDialog->findChild<QCheckBox *>("debuginfodCheckBox");
    QVERIFY(arch && bits && endianness && level && debuginfodBox);
    QVERIFY(debuginfodBox->isChecked());

    const int x86 = arch->findData("x86");
    const int bits32 = bits->findText("32");
    QVERIFY(x86 >= 0);
    QVERIFY(bits32 >= 0);
    arch->setCurrentIndex(x86);
    bits->setCurrentIndex(bits32);
    endianness->setCurrentIndex(1);
    level->setValue(autoAnalysisLevel);
    debuginfodBox->setChecked(false);

    QVERIFY(CutterTest::clickButton(optionsDialog, "okButton"));
    QVERIFY(!CutterTest::core()->getConfigb(debuginfod));

    auto *app = qobject_cast<CutterApplication *>(QCoreApplication::instance());
    QVERIFY(app);
    const InitialOptions options = app->getInitialOptions();
    QVERIFY(options.filename.endsWith(QDir::toNativeSeparators(helloPath)));
    QCOMPARE(options.arch, QString("x86"));
    QCOMPARE(options.bits, 32);
    QCOMPARE(options.endian, InitialOptions::Endianness::Little);
    QCOMPARE(options.analysisCmd.size(), 1);
    QCOMPARE(options.analysisCmd.first().command, QString("aaa"));

    QTRY_VERIFY_WITH_TIMEOUT(CutterTest::mainWindow()->isVisible(), analysisTimeoutMs);
}

CUTTER_APP_TEST_MAIN(TestOpenFile)
#include "test_OpenFile.moc"
