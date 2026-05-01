
#ifndef ASYNCTASK_H
#define ASYNCTASK_H

#include "core/CutterCommon.h"

#include <QRunnable>
#include <QThreadPool>
#include <QMutex>
#include <QElapsedTimer>
#include <QSharedPointer>
#include <QList>

class AsyncTaskManager;

// TODO: Same CUTTER_EXPORT check
// TODO: docs
class CUTTER_EXPORT AsyncTask : public QObject, public QRunnable
{
    Q_OBJECT

    friend class AsyncTaskManager;

public:
    using Ptr = std::shared_ptr<AsyncTask>;

    AsyncTask();
    ~AsyncTask();

    void run() override final;

    void wait();
    bool wait(int timeout);
    virtual void interrupt();
    bool isInterrupted() const { return interrupted; }
    bool isRunning() const { return running; }

    const QString &getLog() { return logBuffer; }
    const QElapsedTimer &getTimer() { return timer; }
    qint64 getElapsedTime() { return timer.isValid() ? timer.elapsed() : 0; }

    virtual QString getTitle() { return QString(); }

protected:
    virtual void runTask() = 0;

    void log(QString s);

signals:
    void finished();
    void logChanged(const QString &log);

private:
    bool running;
    bool interrupted;
    QMutex runningMutex;

    QElapsedTimer timer;
    QString logBuffer;

    void prepareRun();
};

// TODO: docs
class AsyncTaskManager : public QObject
{
    Q_OBJECT

private:
    QThreadPool *threadPool;
    QList<AsyncTask::Ptr> tasks;

public:
    explicit AsyncTaskManager(QObject *parent = nullptr);
    ~AsyncTaskManager();

    void start(const AsyncTask::Ptr &task);
    bool getTasksRunning();

signals:
    void tasksChanged();
};

#endif // ASYNCTASK_H
