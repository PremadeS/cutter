#ifndef ANALYSISTASK_H
#define ANALYSISTASK_H

#include "common/AsyncTask.h"
#include "core/Cutter.h"
#include "common/InitialOptions.h"

class CutterCore;
class MainWindow;
class InitialOptionsDialog;

/**
 * @brief Background task for initial binary analysis and file loading
 */
class AnalysisTask : public AsyncTask
{
    Q_OBJECT

public:
    explicit AnalysisTask();
    ~AnalysisTask();

    QString getTitle() override;

    void setOptions(const InitialOptions &options);

    void interrupt() override;

    bool getOpenFileFailed() const;

protected:
    void runTask() override;

signals:
    void openFileFailed();

private:
    InitialOptions options;

    bool openFailed = false;
};

#endif // ANALYSISTASK_H
