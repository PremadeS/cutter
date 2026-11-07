#include "Basefind.h"

#include "Cutter.h"

bool Basefind::threadCallback(const RzBaseFindThreadInfo *info, void *user)
{
    auto th = reinterpret_cast<Basefind *>(user);
    return th->updateProgress(info);
}

Basefind::Basefind()
    : scores(nullptr),
      continueRun(true)
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
      ,
      mutex(QMutex::Recursive)
#endif
{
    memset(&options, 0, sizeof(RzBaseFindOpt));
}

Basefind::~Basefind()
{
    cancel();
    wait();
    rz_list_free(scores);
}

bool Basefind::setOptions(const RzBaseFindOpt *opts)
{
    mutex.lock();
    options.max_threads = opts->max_threads;
    options.pointer_size = opts->pointer_size;
    options.start_address = opts->start_address;
    options.end_address = opts->end_address;
    options.alignment = opts->alignment;
    options.min_score = opts->min_score;
    options.min_string_len = opts->min_string_len;
    mutex.unlock();

    const QString error = validateOptions(&options);
    if (!error.isEmpty()) {
        qWarning() << error;
        return false;
    }
    return true;
}

QString Basefind::validateOptions(const RzBaseFindOpt *opts)
{
    if (opts->start_address >= opts->end_address) {
        return tr("The start address must be lower than the end address.");
    }
    if (opts->alignment < RZ_BASEFIND_BASE_ALIGNMENT) {
        return tr("The alignment must be at least %1.")
                .arg(QString::asprintf("0x%x", RZ_BASEFIND_BASE_ALIGNMENT));
    }
    if (opts->min_score < 1) {
        return tr("The minimum score must be at least 1.");
    }
    if (opts->min_string_len < 1) {
        return tr("The minimum string length must be at least 1.");
    }
    return {};
}

void Basefind::run()
{
    qRegisterMetaType<BasefindCoreStatusDescription>();

    mutex.lock();
    rz_list_free(scores);
    scores = nullptr;
    continueRun = true;
    mutex.unlock();

    auto core = Core()->lock();
    options.callback = threadCallback;
    options.user = this;
    scores = rz_basefind(core, &options);

    emit complete();
}

void Basefind::cancel()
{
    mutex.lock();
    continueRun = false;
    mutex.unlock();
}

QList<BasefindResultDescription> Basefind::results()
{
    QList<BasefindResultDescription> pairs;
    RzListIter *it;
    RzBaseFindScore *pair;
    CutterRzListForeach (scores, it, RzBaseFindScore, pair) {
        BasefindResultDescription desc;
        desc.candidate = pair->candidate;
        desc.score = pair->score;
        pairs.push_back(desc);
    }
    return pairs;
}

bool Basefind::updateProgress(const RzBaseFindThreadInfo *info)
{
    mutex.lock();

    BasefindCoreStatusDescription status;
    status.index = info->thread_idx;
    status.percentage = info->percentage;

    emit progress(status);
    const bool ret = continueRun;
    mutex.unlock();
    return ret;
}
