#ifndef CUTTER_TEST_DEADLOCK_WATCHDOG_H
#define CUTTER_TEST_DEADLOCK_WATCHDOG_H

#include <QByteArray>
#include <QtGlobal>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace CutterTest {

/**
 * @brief Aborts the test when the scope it guards does not finish in time.
 *
 * The fatal message prints what was guarded and the symbolized stack of every thread: first
 * the one that created the watchdog (the stuck slot), then the others (what it is waiting
 * for). Linux uses addr2line, macOS atos and Windows DbgHelp with the build's PDBs.
 */
class DeadlockWatchdog
{
public:
    DeadlockWatchdog(QByteArray what, std::chrono::milliseconds timeout);
    ~DeadlockWatchdog();
    DeadlockWatchdog(const DeadlockWatchdog &) = delete;
    DeadlockWatchdog &operator=(const DeadlockWatchdog &) = delete;

private:
    [[noreturn]] void expire() const;

    QByteArray what;
    quint64 guarded;
    std::mutex mutex;
    std::condition_variable finished;
    bool done = false;
    std::thread thread;
};

} // namespace CutterTest

#endif // CUTTER_TEST_DEADLOCK_WATCHDOG_H
