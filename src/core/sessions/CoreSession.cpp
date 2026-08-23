#include "CoreSession.h"

#include "common/AsyncTask.h"
#include "common/BasicInstructionHighlighter.h"
#include "common/RizinTask.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include <cassert>
#include <memory>
#include <rz_asm.h>
#include <rz_cmd.h>
#include <rz_socket.h>
#include <sdb.h>

CoreSession::CoreSession(QObject *parent)
    : QObject(parent)
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
      ,
      mutex(QMutex::Recursive)
#endif
{
    wrapper = new RizinWrapper(this);

    auto returnFalse = [] { return false; };
    wrapper->setDebugStateProvider(returnFalse, returnFalse);

    // Initialize graph node highlighter
    bbHighlighter = std::make_unique<BasicBlockHighlighter>();

    // Initialize Async tasks manager
    asyncTaskManager = new AsyncTaskManager(this);

    connect(wrapper, &RizinWrapper::reset, this, &CoreSession::refreshAll);

    // Show a message box if the wrapper logs any message
    connect(wrapper, &RizinWrapper::log, this,
            [](LogLevel level, const QString &title, const QString &text) {
                // cannot directly execute a message box here because background threads aren't
                // allowed to touch GUI. so we just drop an event
                QTimer::singleShot(0, qApp, [=]() {
                    switch (level) {
                    case LogLevel::Error:
                        QMessageBox::critical(nullptr, title, text);
                        break;
                    case LogLevel::Warning:
                        QMessageBox::warning(nullptr, title, text);
                        break;
                    case LogLevel::Info:
                        QMessageBox::information(nullptr, title, text);
                        break;
                    }
                });
            });
}

RizinLocked CoreSession::lock()
{
    return RizinLocked(this);
}

QDir CoreSession::getCutterRCDefaultDirectory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

QVector<QString> CoreSession::getCutterRCFilePaths() const
{
    QVector<QString> result;
    result.push_back(QFileInfo(QDir::home(), ".cutterrc").absoluteFilePath());
    const QStringList locations =
            QStandardPaths::standardLocations(QStandardPaths::AppConfigLocation);
    for (auto &location : locations) {
        result.push_back(QFileInfo(QDir(location), ".cutterrc").absoluteFilePath());
    }
    result.push_back(QFileInfo(getCutterRCDefaultDirectory(), "rc")
                             .absoluteFilePath()); // File in config editor is from this path
    return result;
}

void CoreSession::loadCutterRC()
{
    const auto result = getCutterRCFilePaths();
    for (auto &cutterRCFilePath : result) {
        auto cutterRCFileInfo = QFileInfo(cutterRCFilePath);
        if (!cutterRCFileInfo.exists() || !cutterRCFileInfo.isFile()) {
            continue;
        }
        qInfo() << tr("Loading initialization file from ") << cutterRCFilePath;
        LOCK(this, {
            rz_core_cmd_file(rizin.core(), cutterRCFilePath.toUtf8().constData());
            rz_cons_flush();
        });
    }
}

void CoreSession::loadDefaultCutterRC()
{
    auto cutterRCFilePath = QFileInfo(getCutterRCDefaultDirectory(), "rc").absoluteFilePath();
    const auto cutterRCFileInfo = QFileInfo(cutterRCFilePath);
    if (!cutterRCFileInfo.exists() || !cutterRCFileInfo.isFile()) {
        return;
    }
    qInfo() << tr("Loading initialization file from ") << cutterRCFilePath;
    auto rizin = this->lock();
    rz_core_cmd_file(rizin.core(), cutterRCFilePath.toUtf8().constData());
    rz_cons_flush();
}

bool CoreSession::asyncTask(std::function<void *(RzCore *)> fcn, std::shared_ptr<RizinTask> &task)
{
    if (task) {
        return false;
    }

    auto rizin = this->lock();
    const RVA offset = rizin->getOffset();
    task = std::shared_ptr<RizinTask>(new RizinFunctionTask(std::move(fcn), true));
    connect(task.get(), &RizinTask::finished, task.get(), [this, offset, task]() {
        auto rizin = this->lock();

        if (offset != rizin->getOffset()) {
            rizin->updateSeek();
        }
    });

    return true;
}

void CoreSession::functionTask(std::function<void *(RzCore *)> fcn)
{
    auto task = std::unique_ptr<RizinTask>(new RizinFunctionTask(std::move(fcn), true));
    task->startTask();
    task->joinTask();
}

QString CoreSession::cmdTask(const QString &str)
{
    RizinCmdTask task(str);
    task.startTask();
    task.joinTask();
    return task.getResult();
}

void CoreSession::showMemoryWidget()
{
    emit showMemoryWidgetRequested();
}

void CoreSession::seekAndShow(ut64 offset)
{
    {
        auto rizin = this->lock();
        rizin->seek(offset);
    }
    emit showAddressRequested(offset);
}

void CoreSession::seekAndShow(const QString &offset)
{
    RVA addr;
    {
        auto rizin = this->lock();
        rizin->seek(offset);
        addr = rizin->math(offset);
    }
    emit showAddressRequested(addr);
}

void CoreSession::setGraphEmpty(bool empty)
{
    emptyGraph = empty;
}

bool CoreSession::isGraphEmpty() const
{
    return emptyGraph;
}

void CoreSession::showTypeInTypesWidget(const QString &typeName)
{
    emit showTypeRequested(typeName);
}

void CoreSession::message(const QString &msg, bool debug)
{
    if (msg.isEmpty()) {
        return;
    }
    if (debug) {
        qDebug() << msg;
        emit newDebugMessage(msg);
        return;
    }
    emit newMessage(msg);
}

BasicBlockHighlighter *CoreSession::getBBHighlighter()
{
    return bbHighlighter.get();
}

BasicInstructionHighlighter *CoreSession::getBIHighlighter()
{
    return &biHighlighter;
}

void CoreSession::triggerRefreshAll()
{
    emit refreshAll();
}

void CoreSession::triggerAsmOptionsChanged()
{
    emit asmOptionsChanged();
}

void CoreSession::triggerGraphOptionsChanged()
{
    emit graphOptionsChanged();
}

void CoreSession::triggerDebugOptionsChanged()
{
    emit debugOptionsChanged();
}

void CoreSession::triggerAnalysisOptionsChanged()
{
    emit analysisOptionsChanged();
}

void CoreSession::triggerSymbolsOptionsChanged()
{
    emit symbolsOptionsChanged();
}

RizinLocked::RizinLocked(CoreSession *session) : session(session)
{
    lock();

    // TODO:
    assert(session->wrapper->coreLockDepth >= 0);
    session->wrapper->coreLockDepth++;
    if (session->wrapper->coreLockDepth == 1) {
        assert(session->wrapper->coreBed);
        rz_cons_sleep_end(session->wrapper->coreBed);
        session->wrapper->coreBed = nullptr;
    }
}

RizinLocked::~RizinLocked()
{
    // TODO:
    assert(session->wrapper->coreLockDepth > 0);
    session->wrapper->coreLockDepth--;
    if (session->wrapper->coreLockDepth == 0) {
        session->wrapper->coreBed = rz_cons_sleep_begin();
    }

    unlock();
}

void RizinLocked::unlock()
{
    if (locked) {
        session->mutex.unlock();
        locked = false;
    }
}

void RizinLocked::lock()
{
    if (!locked) {
        session->mutex.lock();
        locked = true;
    }
}

RzCore *RizinLocked::core()
{
    assert(locked && "Cannot access RzCore after unlock(), must lock() before");
    return session->wrapper->core;
}

RizinWrapper *RizinLocked::operator->() &
{
    assert(locked && "Cannot access RizinWrapper after unlock(), must lock() before");
    return session->wrapper;
}