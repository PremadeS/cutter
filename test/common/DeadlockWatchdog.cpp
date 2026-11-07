#include "DeadlockWatchdog.h"

#include <QtGlobal>

#include <utility>

namespace CutterTest {

DeadlockWatchdog::DeadlockWatchdog(QByteArray what, std::chrono::milliseconds timeout)
    : what(std::move(what)),
      thread([this, timeout]() {
          std::unique_lock lock(mutex);
          if (!finished.wait_for(lock, timeout, [this]() { return done; })) {
              qFatal("%s did not finish within %lld ms", this->what.constData(),
                     static_cast<long long>(timeout.count()));
          }
      })
{
}

DeadlockWatchdog::~DeadlockWatchdog()
{
    {
        const std::lock_guard lock(mutex);
        done = true;
    }
    finished.notify_one();
    thread.join();
}

} // namespace CutterTest
