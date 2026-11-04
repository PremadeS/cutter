#include "TestEnv.h"

#include "CutterApplication.h"
#include "common/AsyncTask.h"
#include "core/CutterDescriptions.h"
#include "core/MainWindow.h"

#include <QApplication>
#include <QByteArrayList>
#include <QDeadlineTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>

#include <cstdio>
#include <vector>

static void initCutterResources()
{
    Q_INIT_RESOURCE(resources);
}

namespace {

constexpr int fileLoadTimeoutMs = 120000;
constexpr RVA commentSearchRange = 0x100;

void prepareEnvironment()
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    initCutterResources();

    qRegisterMetaType<QList<StringDescription>>();
    qRegisterMetaType<QList<FunctionDescription>>();

    static QTemporaryDir settingsDir;
    QCoreApplication::setOrganizationName("rizin-tests");
    QCoreApplication::setApplicationName("cutter-tests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());

    QSettings settings;
    settings.setValue("firstExecution", false);
    settings.sync();
}

bool waitForOpenedFile()
{
    const QDeadlineTimer deadline(fileLoadTimeoutMs);
    while (!deadline.hasExpired()) {
        MainWindow *main = CutterTest::mainWindow();
        if (main && main->isVisible()
            && !CutterTest::core()->getAsyncTaskManager()->getTasksRunning()) {
            return true;
        }
        QTest::qWait(50);
    }
    return false;
}

} // namespace

namespace CutterTest {

QString fixturePath(const QString &relativePath)
{
    const QString root = qEnvironmentVariable(testbinsVariable);
    if (root.isEmpty()) {
        return {};
    }
    const QFileInfo info(QDir(root).filePath(relativePath));
    return info.isFile() ? info.absoluteFilePath() : QString();
}

const char *missingFixtureMessage()
{
    static const QByteArray message =
            QByteArray("rizin-testbins not available, set ") + testbinsVariable;
    return message.constData();
}

CutterCore *core()
{
    return CutterCore::instance();
}

MainWindow *mainWindow()
{
    auto *app = qobject_cast<CutterApplication *>(QCoreApplication::instance());
    return app ? app->getMainWindow() : nullptr;
}

bool openFile(const QString &path)
{
    return core()->loadFile(path, 0, 0, RZ_PERM_R, 1, true);
}

void analyze()
{
    core()->cmdRaw("aaa");
}

RVA addressWithoutComment(RVA start)
{
    for (RVA candidate = start; candidate < start + commentSearchRange; ++candidate) {
        if (core()->getCommentAt(candidate).isEmpty()) {
            return candidate;
        }
    }
    return RVA_INVALID;
}

int runWithoutCore(int argc, char **argv, const TestRunner &runner)
{
    prepareEnvironment();

    const QApplication app(argc, argv);

    return runner(argc, argv);
}

int runCoreTest(int argc, char **argv, const TestRunner &runner)
{
    prepareEnvironment();

    const QApplication app(argc, argv);
    CutterCore cutterCore;
    cutterCore.initialize(false);
    cutterCore.setSettings();

    return runner(argc, argv);
}

int runAppTest(int argc, char **argv, const QString &fixture, const TestRunner &runner)
{
    prepareEnvironment();

    QString file;
    if (!fixture.isEmpty()) {
        file = fixturePath(fixture);
        if (file.isEmpty()) {
            std::fprintf(stderr, "SKIP: fixture '%s' not found: %s\n", qPrintable(fixture),
                         missingFixtureMessage());
            return skipReturnCode;
        }
    }

    QByteArrayList cutterArgs = { QByteArray(argv[0]), "--no-output-redirect",
                                  "--no-cutter-plugins", "--no-rizin-plugins" };
    if (!file.isEmpty()) {
        cutterArgs << "-A" << "1" << QFile::encodeName(file);
    }
    std::vector<char *> cutterArgv;
    for (QByteArray &arg : cutterArgs) {
        cutterArgv.push_back(arg.data());
    }
    cutterArgv.push_back(nullptr);
    int cutterArgc = static_cast<int>(cutterArgs.size());

    const CutterApplication app(cutterArgc, cutterArgv.data());

    if (!file.isEmpty() && !waitForOpenedFile()) {
        std::fprintf(stderr, "FAIL: '%s' did not finish loading\n", qPrintable(file));
        return 1;
    }

    return runner(argc, argv);
}

} // namespace CutterTest
