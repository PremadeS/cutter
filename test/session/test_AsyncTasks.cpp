/**
 * @file test_AsyncTasks.cpp
 * @brief Background tasks run through the core's AsyncTaskManager.
 *
 * File loading and analysis run as tasks, and the UI decides what to show from their
 * finished signal and the manager's running state. A task reported twice runs
 * finalizeOpen twice; a task that ignores interrupt freezes Cutter when the user
 * cancels analysis (the core side of that bug).
 */

#include "CallbackTask.h"
#include "Fixtures.h"
#include "TestEnv.h"

#include "common/AnalysisTask.h"

#include <QScopeGuard>
#include <QSemaphore>
#include <QSignalSpy>
#include <QTest>

#include <atomic>
#include <memory>

namespace {

constexpr int taskTimeoutMs = 5000;
constexpr int duplicateSignalWaitMs = 100;
constexpr int analysisStartTimeoutMs = 60000;
constexpr int interruptTimeoutMs = 2000;

} // namespace

class TestAsyncTasks : public QObject
{
    Q_OBJECT

private slots:
    void taskReportsFinishedOnceAndLeavesManager();
    void interruptedAnalysisFinishesQuickly();
};

void TestAsyncTasks::taskReportsFinishedOnceAndLeavesManager()
{
    AsyncTaskManager *manager = CutterTest::core()->getAsyncTaskManager();
    QVERIFY(!manager->getTasksRunning());

    QSemaphore gate;
    std::atomic<bool> started { false };
    auto task = std::make_shared<CutterTest::CallbackTask>([&]() {
        started = true;
        gate.acquire();
    });
    const auto releaseGate = qScopeGuard([&]() { gate.release(); });

    QObject context;
    int finishedCount = 0;
    connect(task.get(), &AsyncTask::finished, &context, [&]() { ++finishedCount; });
    QSignalSpy tasksSpy(manager, &AsyncTaskManager::tasksChanged);

    manager->start(task);
    QVERIFY(manager->getTasksRunning());
    QTRY_VERIFY_WITH_TIMEOUT(started.load(), taskTimeoutMs);
    QCOMPARE(finishedCount, 0);

    gate.release();
    QTRY_COMPARE_WITH_TIMEOUT(finishedCount, 1, taskTimeoutMs);
    QTRY_VERIFY_WITH_TIMEOUT(!manager->getTasksRunning(), taskTimeoutMs);

    QTest::qWait(duplicateSignalWaitMs);
    QCOMPARE(finishedCount, 1);
    QCOMPARE(tasksSpy.count(), 2);
}

void TestAsyncTasks::interruptedAnalysisFinishesQuickly()
{
    const QString path = CutterTest::fixturePath(CutterTest::Fixtures::coreutilsLs());
    if (path.isEmpty()) {
        QSKIP("rizin-testbins not available, set CUTTER_TESTBINS_DIR");
    }

    InitialOptions options;
    options.filename = path;
    options.endian = InitialOptions::Endianness::Auto;
    options.analysisCmd = { { "aaaa", "Auto analysis (experimental)" } };

    auto task = std::make_shared<AnalysisTask>();
    task->setOptions(options);

    QObject context;
    bool analysisStarted = false;
    bool finished = false;
    connect(task.get(), &AsyncTask::logChanged, &context, [&](const QString &log) {
        analysisStarted = analysisStarted || log.contains("Executing analysis");
    });
    connect(task.get(), &AsyncTask::finished, &context, [&]() { finished = true; });

    CutterTest::core()->getAsyncTaskManager()->start(task);
    QTRY_VERIFY_WITH_TIMEOUT(analysisStarted, analysisStartTimeoutMs);
    if (finished) {
        QSKIP("analysis finished before it could be interrupted");
    }

    task->interrupt();
    QTRY_VERIFY_WITH_TIMEOUT(finished, interruptTimeoutMs);
    QVERIFY(!task->getOpenFileFailed());
}

CUTTER_CORE_TEST_MAIN(TestAsyncTasks)
#include "test_AsyncTasks.moc"
