/**
 * @file test_EditVariablesDialog.cpp
 * @brief Renaming and retyping a function's variables through their dialog.
 *
 * The dialog is opened for one function and must apply the edit to that function's
 * variable, with the new name sanitized for rizin. It is often opened from places
 * where the current seek is somewhere else (the Functions list, the decompiler), so
 * the edit must not depend on where the seek happens to be.
 */

#include "Fixtures.h"
#include "TestEnv.h"
#include "UiHelpers.h"

#include "dialogs/EditVariablesDialog.h"

#include <QTest>

namespace {

bool hasVariable(RVA function, const QString &name)
{
    const QList<VariableDescription> variables = CutterTest::core()->getVariables(function);
    for (const VariableDescription &variable : variables) {
        if (variable.name == name) {
            return true;
        }
    }
    return false;
}

QString firstVariable(RVA function)
{
    const QList<VariableDescription> variables = CutterTest::core()->getVariables(function);
    return variables.isEmpty() ? QString() : variables.first().name;
}

bool renameThroughDialog(RVA function, const QString &variable, const QString &newName)
{
    EditVariablesDialog dialog(function, variable);
    return CutterTest::setLineEditText(&dialog, "nameEdit", newName)
            && CutterTest::clickStandardButton(&dialog, QDialogButtonBox::Ok);
}

} // namespace

class TestEditVariablesDialog : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void renameAppliesToVariable();
    void newNameIsSanitized();
    void editAppliesWhenSeekIsInAnotherFunction();

private:
    RVA function = RVA_INVALID;
    RVA otherFunction = RVA_INVALID;
};

void TestEditVariablesDialog::initTestCase()
{
    CUTTER_REQUIRE_FIXTURE(const QString path, CutterTest::Fixtures::helloX64());
    QVERIFY(CutterTest::openFile(path));
    CutterTest::analyze();

    const QList<FunctionDescription> functions = CutterTest::core()->getAllFunctions();
    for (const FunctionDescription &candidate : functions) {
        const bool hasVariables = !firstVariable(candidate.offset).isEmpty();
        if (function == RVA_INVALID && hasVariables) {
            function = candidate.offset;
        } else if (otherFunction == RVA_INVALID && !hasVariables) {
            otherFunction = candidate.offset;
        }
    }
    QVERIFY2(function != RVA_INVALID, "no analyzed function with variables");
    QVERIFY2(otherFunction != RVA_INVALID, "no analyzed function without variables");
}

void TestEditVariablesDialog::init()
{
    CutterTest::core()->seek(function);
}

void TestEditVariablesDialog::renameAppliesToVariable()
{
    const QString oldName = firstVariable(function);
    const QString newName = QStringLiteral("cutter_test_var");

    QVERIFY(renameThroughDialog(function, oldName, newName));
    QVERIFY(hasVariable(function, newName));
    QVERIFY(!hasVariable(function, oldName));
}

void TestEditVariablesDialog::newNameIsSanitized()
{
    const QString oldName = firstVariable(function);

    QVERIFY(renameThroughDialog(function, oldName, "cutter sanitized/name\\var"));
    QVERIFY(hasVariable(function, "cutter_sanitized_name_var"));
}

void TestEditVariablesDialog::editAppliesWhenSeekIsInAnotherFunction()
{
    const QString oldName = firstVariable(function);
    const QString newName = QStringLiteral("cutter_test_var_elsewhere");
    CutterTest::core()->seek(otherFunction);

    QVERIFY(renameThroughDialog(function, oldName, newName));
    QVERIFY(hasVariable(function, newName));
    QVERIFY(!hasVariable(function, oldName));
}

CUTTER_CORE_TEST_MAIN(TestEditVariablesDialog)
#include "test_EditVariablesDialog.moc"
