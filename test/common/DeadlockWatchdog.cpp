#include "DeadlockWatchdog.h"

#include <QDir>
#include <QFile>
#include <QMap>
#include <QProcess>
#include <QStringList>

#include <utility>

#ifdef Q_OS_LINUX
#    include <algorithm>
#    include <atomic>
#    include <csignal>
#    include <dlfcn.h>
#    include <execinfo.h>
#    include <sys/syscall.h>
#    include <unistd.h>
#endif

namespace CutterTest {

namespace {

// print stack trace
#ifdef Q_OS_LINUX

constexpr int maxFrames = 64;
void *capturedFrames[maxFrames];
std::atomic<int> capturedCount { -1 };

void captureStack(int)
{
    capturedCount = backtrace(capturedFrames, maxFrames);
}

struct ThreadRef
{
    quint64 id;
    QString name;
};

quint64 currentThreadId()
{
    return quint64(syscall(SYS_gettid));
}

QList<ThreadRef> allThreads()
{
    QList<ThreadRef> threads;
    const QStringList ids = QDir("/proc/self/task").entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &id : ids) {
        QFile comm(QString("/proc/self/task/%1/comm").arg(id));
        const QString name =
                comm.open(QIODevice::ReadOnly) ? QString::fromUtf8(comm.readAll()).trimmed() : QString();
        threads.append({ id.toULongLong(), name });
    }
    return threads;
}

QList<quintptr> captureStackOf(quint64 thread)
{
    constexpr int captureAttempts = 50;
    constexpr std::chrono::milliseconds captureStep { 10 };

    capturedCount = -1;
    if (syscall(SYS_tgkill, getpid(), pid_t(thread), SIGUSR1) != 0) {
        return {};
    }
    for (int attempt = 0; attempt < captureAttempts && capturedCount < 0; ++attempt) {
        std::this_thread::sleep_for(captureStep);
    }
    QList<quintptr> frames;
    for (int i = 0; i < capturedCount; ++i) {
        frames.append(reinterpret_cast<quintptr>(capturedFrames[i]) - 1);
    }
    return frames;
}

QString hex(quintptr value)
{
    return "0x" + QString::number(value, 16);
}

QStringList symbolize(const QList<quintptr> &frames)
{
    constexpr int toolTimeoutMs = 30000;
    struct Image
    {
        QList<int> frameIndices;
        QStringList offsets;
    };

    QStringList lines;
    QMap<QString, Image> images;
    for (int i = 0; i < frames.size(); ++i) {
        const quintptr address = frames.at(i);
        Dl_info info {};
        if (!dladdr(reinterpret_cast<void *>(address), &info) || !info.dli_fname) {
            lines << hex(address);
            continue;
        }
        const QString file = QString::fromLocal8Bit(info.dli_fname);
        Image &image = images[file];
        image.offsets << hex(address - reinterpret_cast<quintptr>(info.dli_fbase));
        image.frameIndices << i;
        lines << hex(address) + " in " + file;
    }

    for (auto it = images.cbegin(); it != images.cend(); ++it) {
        QProcess tool;
        tool.start("addr2line", QStringList { "-C", "-f", "-p", "-e", it.key() } + it->offsets);
        if (!tool.waitForFinished(toolTimeoutMs) || tool.exitCode() != 0) {
            continue;
        }
        const QStringList output = QString::fromLocal8Bit(tool.readAllStandardOutput())
                                           .split('\n', Qt::SkipEmptyParts);
        for (int k = 0; k < output.size() && k < it->frameIndices.size(); ++k) {
            lines[it->frameIndices.at(k)] = output.at(k).trimmed();
        }
    }
    return lines;
}

QString allStacks(quint64 guarded)
{
    struct sigaction action {};
    action.sa_handler = captureStack;
    sigemptyset(&action.sa_mask);
    sigaction(SIGUSR1, &action, nullptr);

    QList<ThreadRef> threads = allThreads();
    std::stable_partition(threads.begin(), threads.end(),
                          [guarded](const ThreadRef &thread) { return thread.id == guarded; });

    const quint64 self = currentThreadId();
    QList<QString> headers;
    QList<QList<quintptr>> stacks;
    QList<quintptr> allFrames;
    for (const ThreadRef &thread : std::as_const(threads)) {
        if (thread.id == self) {
            continue;
        }
        QString header = thread.id == guarded ? QStringLiteral("Blocked thread")
                                              : QStringLiteral("Thread");
        header += QString(" %1%2:").arg(QString::number(thread.id),
                                        thread.name.isEmpty() ? QString() : " (" + thread.name + ")");
        headers << header;
        stacks << captureStackOf(thread.id);
        allFrames << stacks.last();
    }

    const QStringList symbols = symbolize(allFrames);
    QString text;
    int next = 0;
    for (int t = 0; t < stacks.size(); ++t) {
        text += headers.at(t) + "\n";
        if (stacks.at(t).isEmpty()) {
            text += "  (stack could not be captured)\n";
        }
        for (int f = 0; f < stacks.at(t).size(); ++f) {
            text += "  " + symbols.at(next++) + "\n";
        }
        text += "\n";
    }
    return text;
}

#else

quint64 currentThreadId()
{
    return 0;
}

QString allStacks(quint64)
{
    return {};
}

#endif

} // namespace

DeadlockWatchdog::DeadlockWatchdog(QByteArray what, std::chrono::milliseconds timeout)
    : what(std::move(what)),
      guarded(currentThreadId()),
      thread([this, timeout]() {
          std::unique_lock lock(mutex);
          if (!finished.wait_for(lock, timeout, [this]() { return done; })) {
              expire();
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

void DeadlockWatchdog::expire() const
{
    const QString report =
            QString::fromUtf8(what) + " did not return in time. Stacks:\n\n" + allStacks(guarded);
    qFatal("%s", qPrintable(report));
}

} // namespace CutterTest
