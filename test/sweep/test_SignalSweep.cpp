/**
 * @file test_SignalSweep.cpp
 * @brief Every CutterCore signal emitted under the core lock against a full MainWindow.
 *
 * Core methods emit while holding the core lock, so every slot connected to a core signal runs
 * under it. Emitting each signal twice under the lock with every dock visible finds slots that
 * deadlock by waiting for a task that needs the lock; the watchdog names the stuck slot.
 */

#include "AsyncTask.h"
#include "CutterDockWidget.h"
#include "DeadlockWatchdog.h"
#include "Fixtures.h"
#include "TestEnv.h"
#include "core/MainWindow.h"

#include <QMetaMethod>
#include <QSet>
#include <QTest>

#include <chrono>

namespace {

struct SignalArguments
{
    RVA address = RVA_INVALID;
    QString name;
    bool flag = false;
    int number = 0;
    CutterCore::SeekHistoryType history = CutterCore::SeekHistoryType::New;

    void *pointerFor(const QByteArray &type)
    {
        if (type == "RVA") {
            return &address;
        }
        if (type == "QString") {
            return &name;
        }
        if (type == "bool") {
            return &flag;
        }
        if (type == "int") {
            return &number;
        }
        if (type == "CutterCore::SeekHistoryType") {
            return &history;
        }
        return nullptr;
    }
};

void emitSignal(CutterCore *core, const QMetaMethod &signal, SignalArguments &arguments)
{
    QList<void *> argv = { nullptr };
    for (const QByteArray &type : signal.parameterTypes()) {
        argv.append(arguments.pointerFor(type));
    }
    QMetaObject::metacall(core, QMetaObject::InvokeMetaMethod, signal.methodIndex(), argv.data());
}

bool tasksSettled()
{
    return !CutterTest::core()->getAsyncTaskManager()->getTasksRunning();
}

} // namespace

class TestSignalSweep : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void slotsDoNotDeadlockUnderLock_data();
    void slotsDoNotDeadlockUnderLock();

private:
    SignalArguments arguments;
};

void TestSignalSweep::initTestCase()
{
    constexpr int settleTimeoutMs = 60000;

    MainWindow *main = CutterTest::mainWindow();
    QVERIFY(main);
    main->show();
    for (CutterDockWidget *dock : main->findChildren<CutterDockWidget *>()) {
        dock->setFloating(true);
        dock->toggleDockWidget(true);
    }
    QTRY_VERIFY_WITH_TIMEOUT(tasksSettled(), settleTimeoutMs);

    const QList<FunctionDescription> functions = CutterTest::core()->getAllFunctions();
    QVERIFY2(!functions.isEmpty(), "the fixture has no functions");
    arguments.address = functions.first().offset;
    arguments.name = functions.first().name;
}

void TestSignalSweep::slotsDoNotDeadlockUnderLock_data()
{
    QTest::addColumn<int>("methodIndex");

    const QMetaObject &meta = CutterCore::staticMetaObject;
    for (int index = meta.methodOffset(); index < meta.methodCount(); ++index) {
        const QMetaMethod method = meta.method(index);
        if (method.methodType() == QMetaMethod::Signal) {
            QTest::newRow(method.methodSignature().constData()) << index;
        }
    }
}

void TestSignalSweep::slotsDoNotDeadlockUnderLock()
{
    QFETCH(const int, methodIndex);

    constexpr std::chrono::milliseconds deadlockTimeout { 20000 };
    constexpr int taskStartWaitMs = 100;
    constexpr int settleTimeoutMs = 60000;
    static const QSet<QByteArray> needDebugSession = {
        "toggleDebugView",      "debugTaskStateChanged", "attachedRemote",
        "debugProcessFinished", "switchedThread",        "switchedProcess"
    };

    CutterCore *core = CutterTest::core();
    const QMetaMethod signal = CutterCore::staticMetaObject.method(methodIndex);
    if (needDebugSession.contains(signal.name())) {
        QSKIP("needs a debug session");
    }
    for (const QByteArray &type : signal.parameterTypes()) {
        QVERIFY2(arguments.pointerFor(type),
                 qPrintable(QString("add a value for %1 to SignalArguments").arg(QString::fromUtf8(type))));
    }

    {
        const CutterTest::DeadlockWatchdog watchdog(signal.methodSignature(), deadlockTimeout);
        const RzCoreLocked lock(core);
        emitSignal(core, signal, arguments);
        QTest::qWait(taskStartWaitMs);
        emitSignal(core, signal, arguments);
    }
    QTRY_VERIFY_WITH_TIMEOUT(tasksSettled(), settleTimeoutMs);
}

CUTTER_APP_TEST_MAIN_WITH_FILE(TestSignalSweep, CutterTest::Fixtures::helloX64())
#include "test_SignalSweep.moc"
