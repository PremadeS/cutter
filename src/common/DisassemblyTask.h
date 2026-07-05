#ifndef DISASSEMBLYTASK_H
#define DISASSEMBLYTASK_H

#include "common/AsyncTask.h"
#include "common/TempConfig.h"
#include "core/Cutter.h"

#include <QVector>

class DisassemblyTask : public AsyncTask
{
    Q_OBJECT

public:
    explicit DisassemblyTask(RVA topOffset, int maxLines) : topOffset(topOffset), maxLines(maxLines)
    {
    }

    // QString getTitle() const override { return tr("Fetching Disassembly Context"); }

signals:
    void disassemblyFetchFinished(const QList<DisassemblyLine> &lines);

protected:
    void runTask() override
    {
        if (isInterrupted()) {
            return;
        }

        const RVA fetchStart = Core()->prevOpAddr(topOffset, maxLines * historyMultiplier);

        if (isInterrupted()) {
            return;
        }

        QList<DisassemblyLine> fullLines;
        RVA currentOffset = fetchStart;
        TempConfig tempConfig;
        tempConfig.set("scr.color", COLOR_MODE_16M).set("asm.lines", false);

        for (int i = 0; i < contextMultiplier; ++i) {
            if (isInterrupted()) {
                return;
            }

            const auto chunk = Core()->disassembleLines(currentOffset, maxLines);
            if (chunk.isEmpty()) {
                break;
            }

            fullLines.append(chunk);
            currentOffset = Core()->nextOpAddr(chunk.last().offset, 1);

            // needed????
            if (isInterrupted()) {
                return;
            }
        }

        if (!isInterrupted()) {
            emit disassemblyFetchFinished(fullLines);
        }
    }

private:
    RVA topOffset;
    int maxLines;

    static constexpr uint8_t historyMultiplier = 2;
    static constexpr uint8_t contextMultiplier = 5;
};

#endif