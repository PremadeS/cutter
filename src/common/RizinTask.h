#ifndef RZTASK_H
#define RZTASK_H

#include "core/Cutter.h"
#include <memory>

/**
 * @brief TODO: the brief
 */
class CUTTER_EXPORT RizinTask : public QObject
{
    Q_OBJECT

protected:
    RzCoreTask *task; // TODO: the priavte visibility thing?

    RizinTask() {}
    void taskFinished();

public:
    using Ptr = std::shared_ptr<RizinTask>;

    virtual ~RizinTask();

    void startTask();
    void breakTask();
    void joinTask();

signals:
    void finished();
};

class CUTTER_EXPORT RizinCmdTask : public RizinTask
{
    Q_OBJECT

private:
    static void taskFinishedCallback(const char *, void *user);

public:
    explicit RizinCmdTask(const QString &cmd, bool transient = true);

    QString getResult();
    CutterJson getResultJson();
    const char *getResultRaw();
};

class CUTTER_EXPORT RizinFunctionTask : public RizinTask
{
    Q_OBJECT

private:
    std::function<void *(RzCore *)> fcn;
    void *res;

    static void *runner(RzCore *core, void *user);

public:
    explicit RizinFunctionTask(std::function<void *(RzCore *)> fcn, bool transient = true);

    void *getResult() { return res; }
};

#endif // RZTASK_H
