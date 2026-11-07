/**
 * @file test_BreakpointsDialog.cpp
 * @brief Adding breakpoints from the Breakpoints dialog.
 *
 * A software breakpoint is the architecture's trap instruction, whose size
 * rizin knows. The dialog's Size field is only for hardware breakpoints and
 * must not be forced onto a software one, or adding it fails on architectures
 * whose trap is longer than the default size (4 bytes on arm64).
 */

#include "Fixtures.h"
#include "TestEnv.h"
#include "UiHelpers.h"

#include "common/TempConfig.h"
#include "dialogs/BreakpointsDialog.h"

#include <QScopeGuard>
#include <QTest>

class TestBreakpointsDialog : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    /** @brief A software breakpoint added with the dialog's defaults is created on arm64. */
    void softwareBreakpointWithDefaultsIsCreatedOnArm64();

private:
    RVA address = RVA_INVALID;
};

void TestBreakpointsDialog::initTestCase()
{
    CUTTER_REQUIRE_FIXTURE(const QString path, CutterTest::Fixtures::helloX64());
    QVERIFY(CutterTest::openFile(path));
    address = CutterTest::core()->getOffset();
    QVERIFY(address != RVA_INVALID);
}

void TestBreakpointsDialog::softwareBreakpointWithDefaultsIsCreatedOnArm64()
{
    const QString archKey = QStringLiteral("asm.arch");
    const QString bitsKey = QStringLiteral("asm.bits");
    const char *const arm = "arm";
    const int bits64 = 64;
    CutterCore *core = CutterTest::core();
    TempConfig arm64;
    arm64.set(archKey, arm).set(bitsKey, bits64);
    QVERIFY(core->breakpointIndexAt(address) < 0);
    const auto removeBreakpoint = qScopeGuard([core, this]() { core->delBreakpoint(address); });

    CutterTest::MessageBoxWatcher errors;
    QObject context;
    CutterTest::onceVisible<BreakpointsDialog>(&context, [](BreakpointsDialog *dialog) {
        CutterTest::clickStandardButton(dialog, QDialogButtonBox::Ok);
    });
    BreakpointsDialog::createNewBreakpoint(address);

    QVERIFY2(errors.seenTexts().isEmpty(), qPrintable(errors.seenTexts().join(QLatin1Char('\n'))));
    QVERIFY(core->breakpointIndexAt(address) >= 0);
}

CUTTER_CORE_TEST_MAIN(TestBreakpointsDialog)
#include "test_BreakpointsDialog.moc"
