/**
 * @file test_Debugging.cpp
 * @brief Native debugging commands of CutterCore.
 *
 * Each command runs as a debug task and must leave the debuggee where the user
 * asked it to stop. Runs only on Linux x86-64, where the fixture can execute.
 */

#include "Fixtures.h"
#include "TestEnv.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {

bool waitForDebugTask()
{
    const int debugTaskTimeoutMs = 30000;
    CutterCore *core = CutterTest::core();
    return QTest::qWaitFor([core]() { return !core->isDebugTaskInProgress(); },
                           debugTaskTimeoutMs);
}

int opTypeAt(RVA address)
{
    RzCoreLocked core(CutterTest::core());
    RzAnalysisOp *op = rz_core_analysis_op(core, address, RZ_ANALYSIS_OP_MASK_BASIC);
    const int type = op ? static_cast<int>(op->type) : RZ_ANALYSIS_OP_TYPE_UNK;
    rz_analysis_op_free(op);
    return type;
}

} // namespace

class TestDebugging : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    /** @brief Continue until call stops on the next call instruction, not one step further. */
    void continueUntilCallStopsOnACall();

private:
    QTemporaryDir tempDir;
};

void TestDebugging::initTestCase()
{
#if !defined(Q_OS_LINUX) || !defined(Q_PROCESSOR_X86_64)
    QSKIP("the fixture only runs on Linux x86-64");
#endif
    CUTTER_REQUIRE_FIXTURE(const QString fixture, CutterTest::Fixtures::helloX64());
    QVERIFY(tempDir.isValid());
    const QString copyName = QStringLiteral("debug-fixture");
    const QString path = tempDir.filePath(copyName);
    QVERIFY(QFile::copy(fixture, path));
    QVERIFY(QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    QVERIFY(CutterTest::openFile(path));
}

void TestDebugging::init()
{
    CutterCore *core = CutterTest::core();
    core->startDebug();
    QVERIFY(waitForDebugTask());
    QVERIFY(core->currentlyDebugging);

    const QString entryFlag = QStringLiteral("entry0");
    const RVA entry = core->math(entryFlag);
    QVERIFY(entry != RVA_INVALID);
    core->toggleBreakpoint(entry);
    core->continueDebug();
    QVERIFY(waitForDebugTask());
    core->delBreakpoint(entry);
    QCOMPARE(core->getProgramCounterValue(), entry);
}

void TestDebugging::cleanup()
{
    CutterCore *core = CutterTest::core();
    if (core->currentlyDebugging) {
        core->stopDebug();
    }
}

void TestDebugging::continueUntilCallStopsOnACall()
{
    CutterCore *core = CutterTest::core();
    const RVA entry = core->getProgramCounterValue();
    QVERIFY(opTypeAt(entry) != RZ_ANALYSIS_OP_TYPE_CALL);

    core->continueUntilCall();
    QVERIFY(waitForDebugTask());

    const RVA pc = core->getProgramCounterValue();
    QVERIFY(pc != entry);
    QCOMPARE(opTypeAt(pc), static_cast<int>(RZ_ANALYSIS_OP_TYPE_CALL));
}

CUTTER_CORE_TEST_MAIN(TestDebugging)
#include "test_Debugging.moc"
