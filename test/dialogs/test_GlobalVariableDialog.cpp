/**
 * @file test_GlobalVariableDialog.cpp
 * @brief Adding, modifying and removing global variables through their dialog.
 *
 * Like FlagDialog, one name field means add, modify, delete or nothing depending on
 * whether a global variable already exists at the address. A wrong branch renames
 * or deletes the user's variable, or leaves a stray one behind.
 */

#include "Fixtures.h"
#include "TestEnv.h"
#include "UiHelpers.h"

#include "dialogs/GlobalVariableDialog.h"

#include <QSignalSpy>
#include <QTest>

namespace {

QString globalNameAt(RVA address)
{
    const QList<GlobalDescription> globals = CutterTest::core()->getAllGlobals();
    for (const GlobalDescription &global : globals) {
        if (global.addr == address) {
            return global.name;
        }
    }
    return {};
}

bool submit(GlobalVariableDialog &dialog, const QString &name, const QString &type,
            QDialogButtonBox::StandardButton which)
{
    return CutterTest::setLineEditText(&dialog, "nameEdit", name)
            && CutterTest::setLineEditText(&dialog, "typeEdit", type)
            && CutterTest::clickStandardButton(&dialog, which);
}

QString fieldText(GlobalVariableDialog &dialog, const char *objectName)
{
    auto *edit = dialog.findChild<QLineEdit *>(objectName);
    return edit ? edit->text() : QString();
}

} // namespace

class TestGlobalVariableDialog : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void nameAndTypeAddVariable();
    void prefillsAndRenamesExistingVariable();
    void emptyNameDeletesExistingVariable();
    void emptyNameWithoutVariableDoesNothing();
    void cancelDoesNothing();

private:
    RVA address = RVA_INVALID;
    const QString name = QStringLiteral("cutter_test_global");
    const QString renamed = QStringLiteral("cutter_test_global_renamed");
    const QString type = QStringLiteral("int");
};

void TestGlobalVariableDialog::initTestCase()
{
    CUTTER_REQUIRE_FIXTURE(const QString path, CutterTest::Fixtures::helloX64());
    QVERIFY(CutterTest::openFile(path));
    address = CutterTest::core()->getOffset();
    QVERIFY(address != RVA_INVALID);
}

void TestGlobalVariableDialog::init()
{
    CutterTest::core()->delGlobalVariable(address);
    QVERIFY(globalNameAt(address).isEmpty());
}

void TestGlobalVariableDialog::nameAndTypeAddVariable()
{
    GlobalVariableDialog dialog(address);
    QVERIFY(fieldText(dialog, "nameEdit").isEmpty());

    QVERIFY(submit(dialog, name, type, QDialogButtonBox::Ok));
    QCOMPARE(globalNameAt(address), name);
    QCOMPARE(CutterTest::core()->getGlobalVariableType(address), type);
}

void TestGlobalVariableDialog::prefillsAndRenamesExistingVariable()
{
    CutterTest::core()->addGlobalVariable(address, name, type);

    GlobalVariableDialog dialog(address);
    QCOMPARE(fieldText(dialog, "nameEdit"), name);
    QCOMPARE(fieldText(dialog, "typeEdit"), type);

    QVERIFY(submit(dialog, renamed, type, QDialogButtonBox::Ok));
    QCOMPARE(globalNameAt(address), renamed);
}

void TestGlobalVariableDialog::emptyNameDeletesExistingVariable()
{
    CutterTest::core()->addGlobalVariable(address, name, type);

    GlobalVariableDialog dialog(address);
    QVERIFY(submit(dialog, QString(), type, QDialogButtonBox::Ok));
    QVERIFY(globalNameAt(address).isEmpty());
}

void TestGlobalVariableDialog::emptyNameWithoutVariableDoesNothing()
{
    QSignalSpy spy(CutterTest::core(), &CutterCore::globalVarsChanged);

    GlobalVariableDialog dialog(address);
    QVERIFY(submit(dialog, QString(), type, QDialogButtonBox::Ok));
    QCOMPARE(spy.count(), 0);
    QVERIFY(globalNameAt(address).isEmpty());
}

void TestGlobalVariableDialog::cancelDoesNothing()
{
    CutterTest::core()->addGlobalVariable(address, name, type);
    QSignalSpy spy(CutterTest::core(), &CutterCore::globalVarsChanged);

    GlobalVariableDialog dialog(address);
    QVERIFY(submit(dialog, renamed, type, QDialogButtonBox::Cancel));
    QCOMPARE(spy.count(), 0);
    QCOMPARE(globalNameAt(address), name);
}

CUTTER_CORE_TEST_MAIN(TestGlobalVariableDialog)
#include "test_GlobalVariableDialog.moc"
