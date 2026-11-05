#include "DeadlockWatchdog.h"

#include <QDir>
#include <QFile>
#include <QMap>
#include <QProcess>
#include <QStringList>

#include <algorithm>
#include <utility>

#if defined(Q_OS_LINUX)
#    include <atomic>
#    include <csignal>
#    include <dlfcn.h>
#    include <execinfo.h>
#    include <sys/syscall.h>
#    include <unistd.h>
#elif defined(Q_OS_MACOS)
#    include <dlfcn.h>
#    include <mach/mach.h>
#    include <pthread.h>
#elif defined(Q_OS_WIN)
// clang-format off: windows.h must come before the headers that use its types.
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#    include <dbghelp.h>
#    include <tlhelp32.h>
// clang-format on
#endif

namespace CutterTest {

namespace {

constexpr int maxFrames = 64;

/**
 * A thread of this process. native is what the platform needs to read its stack: the Mach
 * thread port on macOS, unused elsewhere (the id is enough).
 */
struct ThreadRef
{
    quint64 id;
    QString name;
    quintptr native = 0;
};

QString hex(quintptr value)
{
    return "0x" + QString::number(value, 16);
}

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)

/**
 * Turns addresses into "function at file:line" with tool (addr2line or atos), one run per
 * binary. Addresses the tool can't resolve keep their hex value and binary.
 */
QStringList symbolizeWith(const QList<quintptr> &frames, bool atos)
{
    constexpr int toolTimeoutMs = 30000;
    struct Image
    {
        quintptr base = 0;
        QList<int> frameIndices;
        QStringList addresses;
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
        image.base = reinterpret_cast<quintptr>(info.dli_fbase);
        // addr2line wants offsets into the file; atos takes addresses and the load address.
        image.addresses << hex(atos ? address : address - image.base);
        image.frameIndices << i;
        lines << hex(address) + " in " + file;
    }

    for (auto it = images.cbegin(); it != images.cend(); ++it) {
        QProcess tool;
        if (atos) {
            tool.start("atos", QStringList { "-o", it.key(), "-l", hex(it->base) }
                               + it->addresses);
        } else {
            tool.start("addr2line",
                       QStringList { "-C", "-f", "-p", "-e", it.key() } + it->addresses);
        }
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

#endif

#if defined(Q_OS_LINUX)

// Each thread captures its own stack in a signal handler; the watchdog waits for it.
void *capturedFrames[maxFrames];
std::atomic<int> capturedCount { -1 };

void captureStack(int)
{
    capturedCount = backtrace(capturedFrames, maxFrames);
}

quint64 currentThreadId()
{
    return quint64(syscall(SYS_gettid));
}

void prepareCapture()
{
    struct sigaction action {};
    action.sa_handler = captureStack;
    sigemptyset(&action.sa_mask);
    sigaction(SIGUSR1, &action, nullptr);
}

QList<ThreadRef> allThreads()
{
    QList<ThreadRef> threads;
    const QStringList ids = QDir("/proc/self/task").entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &id : ids) {
        QFile comm(QString("/proc/self/task/%1/comm").arg(id));
        const QString name = comm.open(QIODevice::ReadOnly)
                ? QString::fromUtf8(comm.readAll()).trimmed()
                : QString();
        threads.append({ id.toULongLong(), name });
    }
    return threads;
}

QList<quintptr> captureStackOf(const ThreadRef &thread)
{
    constexpr int captureAttempts = 50;
    constexpr std::chrono::milliseconds captureStep { 10 };

    capturedCount = -1;
    if (syscall(SYS_tgkill, getpid(), pid_t(thread.id), SIGUSR1) != 0) {
        return {};
    }
    for (int attempt = 0; attempt < captureAttempts && capturedCount < 0; ++attempt) {
        std::this_thread::sleep_for(captureStep);
    }
    QList<quintptr> frames;
    for (int i = 0; i < capturedCount; ++i) {
        // A return address points after the call; one back is inside it.
        frames.append(reinterpret_cast<quintptr>(capturedFrames[i]) - 1);
    }
    return frames;
}

QStringList symbolize(const QList<quintptr> &frames)
{
    return symbolizeWith(frames, false);
}

#elif defined(Q_OS_MACOS)

quint64 currentThreadId()
{
    uint64_t id = 0;
    pthread_threadid_np(nullptr, &id);
    return id;
}

void prepareCapture() { }

QList<ThreadRef> allThreads()
{
    QList<ThreadRef> threads;
    thread_act_array_t list = nullptr;
    mach_msg_type_number_t count = 0;
    if (task_threads(mach_task_self(), &list, &count) != KERN_SUCCESS) {
        return threads;
    }
    for (mach_msg_type_number_t i = 0; i < count; ++i) {
        thread_identifier_info_data_t info {};
        mach_msg_type_number_t infoCount = THREAD_IDENTIFIER_INFO_COUNT;
        if (thread_info(list[i], THREAD_IDENTIFIER_INFO, reinterpret_cast<thread_info_t>(&info),
                        &infoCount)
            != KERN_SUCCESS) {
            continue;
        }
        char name[64] = {};
        if (pthread_t thread = pthread_from_mach_thread_np(list[i])) {
            pthread_getname_np(thread, name, sizeof(name));
        }
        // The ports stay allocated: the process is about to abort.
        threads.append({ info.thread_id, QString::fromUtf8(name), quintptr(list[i]) });
    }
    return threads;
}

/**
 * Suspends the thread and walks its frame-pointer chain, which every function keeps on Apple
 * platforms. Nothing is allocated while it is suspended: it may hold the allocator's lock.
 */
QList<quintptr> captureStackOf(const ThreadRef &thread)
{
    // User-space addresses fit in 47 bits; higher bits may hold a pointer signature.
    constexpr quintptr addressMask = (quintptr(1) << 47) - 1;
    constexpr quintptr maxFrameSize = 8 * 1024 * 1024;

    const auto port = thread_act_t(thread.native);
    quintptr addresses[maxFrames];
    int count = 0;
    if (thread_suspend(port) != KERN_SUCCESS) {
        return {};
    }
    quintptr pc = 0;
    quintptr fp = 0;
#    if defined(__arm64__)
    arm_thread_state64_t state {};
    mach_msg_type_number_t stateCount = ARM_THREAD_STATE64_COUNT;
    if (thread_get_state(port, ARM_THREAD_STATE64, reinterpret_cast<thread_state_t>(&state),
                         &stateCount)
        == KERN_SUCCESS) {
        pc = quintptr(arm_thread_state64_get_pc(state));
        fp = quintptr(arm_thread_state64_get_fp(state));
    }
#    elif defined(__x86_64__)
    x86_thread_state64_t state {};
    mach_msg_type_number_t stateCount = x86_THREAD_STATE64_COUNT;
    if (thread_get_state(port, x86_THREAD_STATE64, reinterpret_cast<thread_state_t>(&state),
                         &stateCount)
        == KERN_SUCCESS) {
        pc = quintptr(state.__rip);
        fp = quintptr(state.__rbp);
    }
#    endif
    if (pc) {
        addresses[count++] = pc & addressMask;
    }
    // A frame record is { previous frame pointer, return address }; frames grow down, so each
    // previous one is higher.
    while (fp && fp % sizeof(quintptr) == 0 && count < maxFrames) {
        const auto *record = reinterpret_cast<const quintptr *>(fp);
        const quintptr previous = record[0];
        const quintptr returnAddress = record[1] & addressMask;
        if (!returnAddress) {
            break;
        }
        addresses[count++] = returnAddress - 1;
        if (previous <= fp || previous - fp > maxFrameSize) {
            break;
        }
        fp = previous;
    }
    thread_resume(port);

    QList<quintptr> frames;
    for (int i = 0; i < count; ++i) {
        frames.append(addresses[i]);
    }
    return frames;
}

QStringList symbolize(const QList<quintptr> &frames)
{
    return symbolizeWith(frames, true);
}

#elif defined(Q_OS_WIN)

quint64 currentThreadId()
{
    return GetCurrentThreadId();
}

/** DbgHelp isn't thread-safe; only the watchdog's thread calls it, after this. */
void prepareCapture()
{
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS);
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);
}

/** GetThreadDescription is looked up at run time: older Windows 10 builds lack it. */
QString threadName(HANDLE thread)
{
    using GetDescription = HRESULT(WINAPI *)(HANDLE, PWSTR *);
    const auto get = reinterpret_cast<GetDescription>(reinterpret_cast<void *>(
            GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetThreadDescription")));
    PWSTR description = nullptr;
    if (!get || FAILED(get(thread, &description)) || !description) {
        return {};
    }
    const QString name = QString::fromWCharArray(description);
    LocalFree(description);
    return name;
}

QList<ThreadRef> allThreads()
{
    QList<ThreadRef> threads;
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return threads;
    }
    const DWORD process = GetCurrentProcessId();
    THREADENTRY32 entry {};
    entry.dwSize = sizeof(entry);
    for (BOOL more = Thread32First(snapshot, &entry); more; more = Thread32Next(snapshot, &entry)) {
        if (entry.th32OwnerProcessID != process) {
            continue;
        }
        QString name;
        if (HANDLE thread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE,
                                       entry.th32ThreadID)) {
            name = threadName(thread);
            CloseHandle(thread);
        }
        threads.append({ entry.th32ThreadID, name });
    }
    CloseHandle(snapshot);
    return threads;
}

/**
 * Suspends the thread and walks its stack with DbgHelp. Nothing is allocated while it is
 * suspended: it may hold the heap's lock.
 */
QList<quintptr> captureStackOf(const ThreadRef &thread)
{
    const HANDLE handle =
            OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION,
                       FALSE, DWORD(thread.id));
    if (!handle) {
        return {};
    }
    DWORD64 addresses[maxFrames];
    int count = 0;
    if (SuspendThread(handle) != DWORD(-1)) {
        CONTEXT context {};
        context.ContextFlags = CONTEXT_FULL;
        if (GetThreadContext(handle, &context)) {
            STACKFRAME64 frame {};
#    if defined(_M_ARM64)
            const DWORD machine = IMAGE_FILE_MACHINE_ARM64;
            frame.AddrPC.Offset = context.Pc;
            frame.AddrFrame.Offset = context.Fp;
            frame.AddrStack.Offset = context.Sp;
#    else
            const DWORD machine = IMAGE_FILE_MACHINE_AMD64;
            frame.AddrPC.Offset = context.Rip;
            frame.AddrFrame.Offset = context.Rbp;
            frame.AddrStack.Offset = context.Rsp;
#    endif
            frame.AddrPC.Mode = AddrModeFlat;
            frame.AddrFrame.Mode = AddrModeFlat;
            frame.AddrStack.Mode = AddrModeFlat;
            while (count < maxFrames
                   && StackWalk64(machine, GetCurrentProcess(), handle, &frame, &context,
                                  nullptr, SymFunctionTableAccess64, SymGetModuleBase64,
                                  nullptr)
                   && frame.AddrPC.Offset) {
                addresses[count++] = frame.AddrPC.Offset;
            }
        }
        ResumeThread(handle);
    }
    CloseHandle(handle);

    QList<quintptr> frames;
    for (int i = 0; i < count; ++i) {
        frames.append(quintptr(addresses[i]));
    }
    return frames;
}

QStringList symbolize(const QList<quintptr> &frames)
{
    const HANDLE process = GetCurrentProcess();
    QStringList lines;
    for (const quintptr address : frames) {
        alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
        auto *symbol = reinterpret_cast<SYMBOL_INFO *>(buffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 displacement = 0;
        QString line = SymFromAddr(process, address, &displacement, symbol)
                ? QString::fromLocal8Bit(symbol->Name)
                : hex(address);
        IMAGEHLP_LINE64 source {};
        source.SizeOfStruct = sizeof(source);
        DWORD lineDisplacement = 0;
        if (SymGetLineFromAddr64(process, address, &lineDisplacement, &source)) {
            line += QString(" at %1:%2")
                            .arg(QString::fromLocal8Bit(source.FileName))
                            .arg(source.LineNumber);
        }
        lines << line;
    }
    return lines;
}

#else

quint64 currentThreadId()
{
    return 0;
}

void prepareCapture() { }

QList<ThreadRef> allThreads()
{
    return {};
}

QList<quintptr> captureStackOf(const ThreadRef &)
{
    return {};
}

QStringList symbolize(const QList<quintptr> &frames)
{
    QStringList lines;
    for (const quintptr address : frames) {
        lines << hex(address);
    }
    return lines;
}

#endif

/** Every thread's stack but the watchdog's own, the guarded thread first. */
QString allStacks(quint64 guarded)
{
    prepareCapture();
    QList<ThreadRef> threads = allThreads();
    std::stable_partition(threads.begin(), threads.end(),
                          [guarded](const ThreadRef &thread) { return thread.id == guarded; });

    const quint64 self = currentThreadId();
    QStringList headers;
    QList<QList<quintptr>> stacks;
    QList<quintptr> allFrames;
    for (const ThreadRef &thread : std::as_const(threads)) {
        if (thread.id == self) {
            continue;
        }
        QString header = thread.id == guarded ? QStringLiteral("Blocked thread")
                                              : QStringLiteral("Thread");
        header += QString(" %1%2:").arg(QString::number(thread.id),
                                        thread.name.isEmpty() ? QString()
                                                              : " (" + thread.name + ")");
        headers << header;
        stacks << captureStackOf(thread);
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
