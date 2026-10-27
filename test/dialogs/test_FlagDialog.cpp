/**
 * @file test_FlagDialog.cpp
 * @brief Adding, renaming and removing flags through the flag dialog.
 *
 * FlagDialog turns one name field into four different actions depending on whether
 * a flag already exists at the address: add, rename, delete, or nothing. Getting
 * that table wrong renames the wrong flag or leaves stray ones behind.
 */

#include "Fixtures.h"
#include "TestEnv.h"
#include "UiHelpers.h"

#include "dialogs/FlagDialog.h"

#include <QSignalSpy>
#include <QTest>

namespace {

constexpr RVA flagSearchRange = 0x100;

bool hasFlag(RVA address, const QString &name)
{
    const QStringList flags =
            CutterTest::core()->listFlagsAsStringAt(address).split(',', Qt::SkipEmptyParts);
    for (const QString &flag : flags) {
        if (flag.trimmed() == name) {
            return true;
        }
    }
    return false;
}

RVA addressWithoutFlag(RVA start)
{
    for (RVA candidate = start; candidate < start + flagSearchRange; ++candidate) {
        if (CutterTest::core()->listFlagsAsStringAt(candidate).isEmpty()) {
            return candidate;
        }
    }
    return RVA_INVALID;
}

QString prefilledName(FlagDialog &dialog)
{
    auto *edit = dialog.findChild<QLineEdit *>("nameEdit");
    return edit ? edit->text() : QString();
}

bool submit(FlagDialog &dialog, const QString &name, QDialogButtonBox::StandardButton which)
{
    return CutterTest::setLineEditText(&dialog, "nameEdit", name)
            && CutterTest::setLineEditText(&dialog, "sizeEdit", "1")
            && CutterTest::clickStandardButton(&dialog, which);
}

} // namespace

class TestFlagDialog : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void nameAddsFlag();
    void prefillsAndRenamesExistingFlag();
    void emptyNameDeletesExistingFlag();
    void emptyNameWithoutFlagDoesNothing();
    void cancelDoesNothing();

private:
    RVA address = RVA_INVALID;
    const QString flagName = QStringLiteral("cutter_test_flag_a");
    const QString renamedFlag = QStringLiteral("cutter_test_renamed");
};

void TestFlagDialog::initTestCase()
{
    const QString path = CutterTest::fixturePath(CutterTest::Fixtures::helloX64());
    if (path.isEmpty()) {
        QSKIP("rizin-testbins not available, set CUTTER_TESTBINS_DIR");
    }
    QVERIFY(CutterTest::openFile(path));

    address = addressWithoutFlag(CutterTest::core()->getOffset());
    QVERIFY2(address != RVA_INVALID, "no address without a flag near the current seek");
}

void TestFlagDialog::init()
{
    CutterCore *core = CutterTest::core();
    core->delFlag(flagName);
    core->delFlag(renamedFlag);
    QVERIFY(core->listFlagsAsStringAt(address).isEmpty());
}

void TestFlagDialog::nameAddsFlag()
{
    FlagDialog dialog(address);
    QVERIFY(prefilledName(dialog).isEmpty());

    QVERIFY(submit(dialog, flagName, QDialogButtonBox::Ok));
    QVERIFY(hasFlag(address, flagName));
}

void TestFlagDialog::prefillsAndRenamesExistingFlag()
{
    CutterTest::core()->addFlag(address, flagName, 1);

    FlagDialog dialog(address);
    QCOMPARE(prefilledName(dialog), flagName);

    QVERIFY(submit(dialog, renamedFlag, QDialogButtonBox::Ok));
    QVERIFY(hasFlag(address, renamedFlag));
    QVERIFY(!hasFlag(address, flagName));
}

void TestFlagDialog::emptyNameDeletesExistingFlag()
{
    CutterTest::core()->addFlag(address, flagName, 1);

    FlagDialog dialog(address);
    QVERIFY(submit(dialog, QString(), QDialogButtonBox::Ok));
    QVERIFY(!hasFlag(address, flagName));
}

void TestFlagDialog::emptyNameWithoutFlagDoesNothing()
{
    QSignalSpy spy(CutterTest::core(), &CutterCore::flagsChanged);

    FlagDialog dialog(address);
    QVERIFY(submit(dialog, QString(), QDialogButtonBox::Ok));
    QCOMPARE(spy.count(), 0);
    QVERIFY(CutterTest::core()->listFlagsAsStringAt(address).isEmpty());
}

void TestFlagDialog::cancelDoesNothing()
{
    CutterTest::core()->addFlag(address, flagName, 1);
    QSignalSpy spy(CutterTest::core(), &CutterCore::flagsChanged);

    FlagDialog dialog(address);
    QVERIFY(submit(dialog, renamedFlag, QDialogButtonBox::Cancel));
    QCOMPARE(spy.count(), 0);
    QVERIFY(hasFlag(address, flagName));
    QVERIFY(!hasFlag(address, renamedFlag));
}

CUTTER_CORE_TEST_MAIN(TestFlagDialog)
#include "test_FlagDialog.moc"
