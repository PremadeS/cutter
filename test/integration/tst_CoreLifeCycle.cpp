#include "Configuration.h"
#include "Cutter.h"
#include "TempConfig.h"

#include <QColor>
#include <QFile>
#include <QTemporaryFile>
#include <QTest>

class TestCoreLifecycle : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testCoreInstanceAndReentrantLocking();
    void testFileLoadingAndSeekHistory();
    void testFlagAndFunctionLifecycle();
    void testCommentsAndBlendedMarks();
    void testAssemblyAndByteEditing();
    void testIOModeAndCacheTransitions();
    void testSdbStorageAndTempConfig();
    void cleanupTestCase();

private:
    QString createExecutableDummyBinary();
    QString m_tempBinaryPath;
};

void TestCoreLifecycle::initTestCase()
{
    // Initialize CutterCore if it hasn't been initialized yet
    if (!CutterCore::instance()) {
        auto *core = new CutterCore(this);
        core->initialize(false); // Do not load external plugins for tests
    }
    QVERIFY(Core() != nullptr);

    m_tempBinaryPath = createExecutableDummyBinary();
    QVERIFY(!m_tempBinaryPath.isEmpty());
}

QString TestCoreLifecycle::createExecutableDummyBinary()
{
    QTemporaryFile tempFile;
    tempFile.setAutoRemove(false);
    if (tempFile.open()) {
        // Simple raw x86_64 buffer: nop; nop; nop; ret
        const char code[] = { '\x90', '\x90', '\x90', '\xc3' };
        tempFile.write(code, sizeof(code));
        tempFile.close();
        return tempFile.fileName();
    }
    return QString();
}

void TestCoreLifecycle::testCoreInstanceAndReentrantLocking()
{
    // Test operator RzCore* and operator->
    {
        RzCoreLocked core1 = Core()->lock();
        RzCore *rawCore1 = static_cast<RzCore *>(core1);
        QVERIFY(rawCore1 != nullptr);
        QVERIFY(core1->analysis != nullptr);

        // Verify re-entrant locking (nested CORE_LOCK)
        {
            RzCoreLocked core2 = Core()->lock();
            RzCore *rawCore2 = static_cast<RzCore *>(core2);
            QCOMPARE(rawCore1, rawCore2);
        }
    }
}

void TestCoreLifecycle::testFileLoadingAndSeekHistory()
{
    // Load binary file
    QVERIFY(Core()->loadFile(m_tempBinaryPath));

    const RVA startAddr = 0x0;
    Core()->seek(startAddr);
    QCOMPARE(Core()->getOffset(), startAddr);

    // Perform a series of seeks to build history
    const RVA target1 = 0x1;
    const RVA target2 = 0x2;

    Core()->seek(target1);
    QCOMPARE(Core()->getOffset(), target1);

    Core()->seek(target2);
    QCOMPARE(Core()->getOffset(), target2);

    // Test Seek Undo / Redo
    Core()->seekPrev();
    QCOMPARE(Core()->getOffset(), target1);

    Core()->seekNext();
    QCOMPARE(Core()->getOffset(), target2);

    // Verify seek history list contains recorded offsets
    QList<RVA> history = Core()->getSeekHistory();
    QVERIFY(!history.isEmpty());
}

void TestCoreLifecycle::testFlagAndFunctionLifecycle()
{
    const RVA testAddr = 0x0;

    // 1. Flag operations
    Core()->addFlag(testAddr, "entry_test_flag", 1);
    QCOMPARE(Core()->flagAt(testAddr), QString("entry_test_flag"));

    Core()->renameFlag("entry_test_flag", "entry_renamed_flag");
    QCOMPARE(Core()->flagAt(testAddr), QString("entry_renamed_flag"));

    // 2. Function operations
    Core()->createFunctionAt(testAddr, "fcn_test_func");
    RzAnalysisFunction *fcn = Core()->functionIn(testAddr);
    QVERIFY(fcn != nullptr);

    Core()->renameFunction(testAddr, "fcn_renamed_func");
    fcn = Core()->functionIn(testAddr);
    QVERIFY(fcn != nullptr);
    QCOMPARE(QString::fromUtf8(fcn->name), QString("fcn_renamed_func"));

    // 3. Cleanup function & flag
    Core()->delFunction(testAddr);
    QVERIFY(Core()->functionIn(testAddr) == nullptr);

    Core()->delFlag("entry_renamed_flag");
    QVERIFY(Core()->flagAt(testAddr).isEmpty());
}

void TestCoreLifecycle::testCommentsAndBlendedMarks()
{
    const RVA commentAddr = 0x1;

    // Comment test
    Core()->setComment(commentAddr, "Test instruction comment");
    QCOMPARE(Core()->getCommentAt(commentAddr), QString("Test instruction comment"));

    Core()->delComment(commentAddr);
    QVERIFY(Core()->getCommentAt(commentAddr).isEmpty());

    // Marks and Alpha Blending test
    const RVA markAddr = 0x0;
    Core()->addMark(markAddr, markAddr + 2, "mark1", "First Mark", Qt::red);
    Core()->addMark(markAddr, markAddr + 2, "mark2", "Second Mark", Qt::blue);

    QList<MarkDescription> marks = Core()->getMarksAt(markAddr);
    QCOMPARE(marks.size(), 2);

    QColor blended = Core()->getBlendedMarksColorAt(markAddr);
    QVERIFY(blended.isValid());

    Core()->delMark("mark1");
    Core()->delMark("mark2");
    QVERIFY(Core()->getMarksAt(markAddr).isEmpty());
}

void TestCoreLifecycle::testAssemblyAndByteEditing()
{
    // Assemble x86_64 instruction
    Core()->setConfig("asm.arch", "x86");
    Core()->setConfig("asm.bits", 64);

    QByteArray bytes = Core()->assemble("nop");
    QVERIFY(!bytes.isEmpty());
    QCOMPARE(static_cast<uchar>(bytes.at(0)), static_cast<uchar>(0x90));

    // Edit bytes in file buffer
    const RVA editAddr = 0x0;
    Core()->editBytes(editAddr, "9090");
    QCOMPARE(Core()->getInstructionBytes(editAddr), QString("9090"));

    // NOP instruction
    Core()->nopInstruction(editAddr);
    QCOMPARE(Core()->getInstructionBytes(editAddr), QString("90"));
}

void TestCoreLifecycle::testIOModeAndCacheTransitions()
{
    // Test write mode toggling
    Core()->setWriteMode(true);
    QVERIFY(Core()->isWriteModeEnabled());

    Core()->setWriteMode(false);
    QVERIFY(!Core()->isWriteModeEnabled());

    // Test IO Cache mode
    Core()->setIOCache(true);
    QVERIFY(Core()->isIOCacheEnabled());

    Core()->setIOCache(false);
    QVERIFY(!Core()->isIOCacheEnabled());
}

void TestCoreLifecycle::testSdbStorageAndTempConfig()
{
    // SDB Namespace and Key-Value test
    const QString sdbNs = "cutter.test.ns";
    const QString sdbKey = "test_key";
    const QString sdbVal = "test_value_123";

    QVERIFY(Core()->sdbSet(sdbNs, sdbKey, sdbVal));
    QCOMPARE(Core()->sdbGet(sdbNs, sdbKey), sdbVal);

    QList<QString> keys = Core()->sdbListKeys(sdbNs);
    QVERIFY(keys.contains(sdbKey));

    // TempConfig Scope Override test
    const QString originalArch = Core()->getConfig("asm.arch");
    {
        TempConfig tempConfig;
        tempConfig.set("asm.arch", "arm");
        QCOMPARE(Core()->getConfig("asm.arch"), QString("arm"));
    }
    // Automatically reverted after exiting block scope
    QCOMPARE(Core()->getConfig("asm.arch"), originalArch);
}

void TestCoreLifecycle::cleanupTestCase()
{
    if (Core()) {
        Core()->cmd("o-*");
    }
    if (!m_tempBinaryPath.isEmpty()) {
        QFile::remove(m_tempBinaryPath);
    }
}

QTEST_MAIN(TestCoreLifecycle)
#include "tst_CoreLifeCycle.moc"