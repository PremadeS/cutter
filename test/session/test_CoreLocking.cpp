/**
 * @file test_CoreLocking.cpp
 * @brief Calling back into the core from signal handlers and background tasks.
 *
 * Core methods emit while holding the core lock, and slots routinely read the core
 * again or start background work. The CoreSession split moves that lock and may
 * replace the recursive mutex, so these tests make sure neither pattern deadlocks
 * or shows listeners stale data.
 */

#include "CallbackTask.h"
#include "Fixtures.h"
#include "TestEnv.h"

#include <QTest>

#include <atomic>
#include <memory>

namespace {

constexpr int taskTimeoutMs = 5000;

} // namespace

class TestCoreLocking : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void slotCanReadCoreDuringSignal();
    void taskStartedFromSlotRunsAfterSignal();

private:
    RVA address = RVA_INVALID;
};

void TestCoreLocking::initTestCase()
{
    const QString path = CutterTest::fixturePath(CutterTest::Fixtures::helloX64());
    if (path.isEmpty()) {
        QSKIP("rizin-testbins not available, set CUTTER_TESTBINS_DIR");
    }
    QVERIFY(CutterTest::openFile(path));

    address = CutterTest::addressWithoutComment(CutterTest::core()->getOffset());
    QVERIFY2(address != RVA_INVALID, "no address without a comment near the current seek");
}

void TestCoreLocking::init()
{
    CutterTest::core()->delComment(address);
}

// valuable if we ever switch from recursive mutex to plain, inside CutterCore
void TestCoreLocking::slotCanReadCoreDuringSignal()
{
    CutterCore *core = CutterTest::core();
    const QString comment = QStringLiteral("read from slot");

    QObject context;
    QString seen;
    connect(core, &CutterCore::commentsChanged, &context,
            [&](RVA changed) { seen = core->getCommentAt(changed); });

    core->setComment(address, comment);
    QCOMPARE(seen, comment);
}

void TestCoreLocking::taskStartedFromSlotRunsAfterSignal()
{
    CutterCore *core = CutterTest::core();
    const QString comment = QStringLiteral("read from task");

    std::atomic<bool> done { false };
    QString seenByTask;
    auto task = std::make_shared<CutterTest::CallbackTask>([&]() {
        seenByTask = core->getCommentAt(address);
        done = true;
    });

    QObject context;
    connect(
            core, &CutterCore::commentsChanged, &context,
            [&]() { core->getAsyncTaskManager()->start(task); }, Qt::SingleShotConnection);

    core->setComment(address, comment);
    QTRY_VERIFY_WITH_TIMEOUT(done.load(), taskTimeoutMs);
    QCOMPARE(seenByTask, comment);
}

CUTTER_CORE_TEST_MAIN(TestCoreLocking)
#include "test_CoreLocking.moc"
