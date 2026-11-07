#ifndef CUTTER_TEST_ENV_H
#define CUTTER_TEST_ENV_H

#include "core/Cutter.h"

#include <QString>
#include <QTest>

#include <functional>

class MainWindow;

namespace CutterTest {

constexpr int skipReturnCode = 77; // TODO: wat dis
inline constexpr char testbinsVariable[] = "CUTTER_TESTBINS_DIR";

using TestRunner = std::function<int(int, char **)>;

QString fixturePath(const QString &relativePath);
const char *missingFixtureMessage();

CutterCore *core();
MainWindow *mainWindow();

bool openFile(const QString &path);
void analyze();
RVA addressWithoutComment(RVA start);

int runWithoutCore(int argc, char **argv, const TestRunner &runner);
int runCoreTest(int argc, char **argv, const TestRunner &runner);
int runAppTest(int argc, char **argv, const QString &fixture, const TestRunner &runner);

} // namespace CutterTest

#define CUTTER_REQUIRE_FIXTURE(target, fixture)                                                    \
    if (CutterTest::fixturePath(fixture).isEmpty()) {                                              \
        QSKIP(CutterTest::missingFixtureMessage());                                                \
    }                                                                                              \
    target = CutterTest::fixturePath(fixture)

#define CUTTER_NO_CORE_TEST_MAIN(TestClass)                                                        \
    int main(int argc, char *argv[])                                                               \
    {                                                                                              \
        return CutterTest::runWithoutCore(argc, argv, [](int testArgc, char **testArgv) {          \
            TestClass test;                                                                        \
            return QTest::qExec(&test, testArgc, testArgv);                                        \
        });                                                                                        \
    }

#define CUTTER_CORE_TEST_MAIN(TestClass)                                                           \
    int main(int argc, char *argv[])                                                               \
    {                                                                                              \
        return CutterTest::runCoreTest(argc, argv, [](int testArgc, char **testArgv) {             \
            TestClass test;                                                                        \
            return QTest::qExec(&test, testArgc, testArgv);                                        \
        });                                                                                        \
    }

#define CUTTER_APP_TEST_MAIN_WITH_FILE(TestClass, fixture)                                         \
    int main(int argc, char *argv[])                                                               \
    {                                                                                              \
        return CutterTest::runAppTest(argc, argv, fixture, [](int testArgc, char **testArgv) {     \
            TestClass test;                                                                        \
            return QTest::qExec(&test, testArgc, testArgv);                                        \
        });                                                                                        \
    }

#define CUTTER_APP_TEST_MAIN(TestClass) CUTTER_APP_TEST_MAIN_WITH_FILE(TestClass, QString())

#endif // CUTTER_TEST_ENV_H
