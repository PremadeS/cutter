#ifndef CUTTER_TEST_DEADLOCK_WATCHDOG_H
#define CUTTER_TEST_DEADLOCK_WATCHDOG_H

#include <QByteArray>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace CutterTest {

/**
 * @brief Aborts the test when the scope it guards doesn't finish in time.
 *
 * The fatal message names what was guarded, so the failing test says what hung.
 */
class DeadlockWatchdog
{
public:
    DeadlockWatchdog(QByteArray what, std::chrono::milliseconds timeout);
    ~DeadlockWatchdog();
    DeadlockWatchdog(const DeadlockWatchdog &) = delete;
    DeadlockWatchdog &operator=(const DeadlockWatchdog &) = delete;

private:
    QByteArray what;
    std::mutex mutex;
    std::condition_variable finished;
    bool done = false;
    std::thread thread;
};

} // namespace CutterTest

#endif // CUTTER_TEST_DEADLOCK_WATCHDOG_H
