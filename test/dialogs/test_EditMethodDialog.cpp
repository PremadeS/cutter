/**
 * @file test_EditMethodDialog.cpp
 * @brief Creating and editing class methods through the method dialog.
 *
 * The dialog requires both a name and a real name, can derive the name from the
 * real name, and editMethod renames the method before updating it when the name
 * changed. A gap here creates nameless methods or leaves the old method behind.
 */

#include "Fixtures.h"
#include "TestEnv.h"
#include "UiHelpers.h"

#include "dialogs/EditMethodDialog.h"

#include <QCheckBox>
#include <QTest>

namespace {

const AnalysisMethodDescription *findMethod(const QList<AnalysisMethodDescription> &methods,
                                            const QString &name)
{
    for (const AnalysisMethodDescription &method : methods) {
        if (method.name == name) {
            return &method;
        }
    }
    return nullptr;
}

bool setAutoRename(QWidget *dialog, bool enabled)
{
    auto *box = dialog->findChild<QCheckBox *>("autoRenameCheckBox");
    if (!box) {
        return false;
    }
    box->setChecked(enabled);
    return box->isChecked() == enabled;
}

QLineEdit *field(QWidget *dialog, const char *objectName)
{
    return dialog->findChild<QLineEdit *>(objectName);
}

bool okEnabled(QWidget *dialog)
{
    auto *buttons = dialog->findChild<QDialogButtonBox *>();
    QPushButton *ok = buttons ? buttons->button(QDialogButtonBox::Ok) : nullptr;
    return ok && ok->isEnabled();
}

} // namespace

class TestEditMethodDialog : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void okNeedsNameAndRealName();
    void autoRenameFollowsRealNameOnlyWhenEnabled();
    void newMethodAddsToClassAtCurrentOffset();
    void editMethodRenamesExistingMethod();

private:
    const QString className = QStringLiteral("CutterTestClass");
};

void TestEditMethodDialog::initTestCase()
{
    CUTTER_REQUIRE_FIXTURE(const QString path, CutterTest::Fixtures::helloX64());
    QVERIFY(CutterTest::openFile(path));
    CutterTest::core()->createNewClass(className);
    QVERIFY(CutterTest::core()->getAllAnalysisClasses(false).contains(className));
}

void TestEditMethodDialog::okNeedsNameAndRealName()
{
    EditMethodDialog dialog(false);
    QVERIFY(setAutoRename(&dialog, false));
    QLineEdit *name = field(&dialog, "nameEdit");
    QLineEdit *realName = field(&dialog, "realNameEdit");
    QVERIFY(name && realName);

    name->setText(QString());
    realName->setText(QString());
    QVERIFY(!okEnabled(&dialog));

    realName->setText("cutter_real");
    QVERIFY(!okEnabled(&dialog));

    name->setText("cutter_name");
    QVERIFY(okEnabled(&dialog));

    realName->setText(QString());
    QVERIFY(!okEnabled(&dialog));
}

void TestEditMethodDialog::autoRenameFollowsRealNameOnlyWhenEnabled()
{
    EditMethodDialog dialog(false);
    QLineEdit *name = field(&dialog, "nameEdit");
    QLineEdit *realName = field(&dialog, "realNameEdit");
    QVERIFY(name && realName);

    QVERIFY(setAutoRename(&dialog, true));
    realName->setText("operator new");
    QVERIFY(!name->text().isEmpty());
    QVERIFY(!name->text().contains(' '));
    QVERIFY(!name->isEnabled());

    QVERIFY(setAutoRename(&dialog, false));
    name->setText("cutter_manual");
    realName->setText("something else");
    QCOMPARE(name->text(), QString("cutter_manual"));
    QVERIFY(name->isEnabled());
}

void TestEditMethodDialog::newMethodAddsToClassAtCurrentOffset()
{
    const QString methodName = QStringLiteral("cutter_new_method");

    QObject context;
    CutterTest::onceVisible<EditMethodDialog>(&context, [&](EditMethodDialog *dialog) {
        setAutoRename(dialog, false);
        CutterTest::setLineEditText(dialog, "nameEdit", methodName);
        CutterTest::setLineEditText(dialog, "realNameEdit", methodName);
        CutterTest::clickStandardButton(dialog, QDialogButtonBox::Ok);
    });
    EditMethodDialog::newMethod(className, QString(), nullptr);

    const QList<AnalysisMethodDescription> methods =
            CutterTest::core()->getAnalysisClassMethods(className);
    const AnalysisMethodDescription *method = findMethod(methods, methodName);
    QVERIFY(method);
    QCOMPARE(method->addr, CutterTest::core()->getOffset());
}

void TestEditMethodDialog::editMethodRenamesExistingMethod()
{
    const QString oldName = QStringLiteral("cutter_old_method");
    const QString newName = QStringLiteral("cutter_renamed_method");
    AnalysisMethodDescription desc;
    desc.name = oldName;
    desc.realName = oldName;
    desc.addr = CutterTest::core()->getOffset();
    desc.vtableOffset = -1;
    CutterTest::core()->setAnalysisMethod(className, desc);

    QObject context;
    CutterTest::onceVisible<EditMethodDialog>(&context, [&](EditMethodDialog *dialog) {
        setAutoRename(dialog, false);
        CutterTest::setLineEditText(dialog, "nameEdit", newName);
        CutterTest::clickStandardButton(dialog, QDialogButtonBox::Ok);
    });
    EditMethodDialog::editMethod(className, oldName, nullptr);

    const QList<AnalysisMethodDescription> methods =
            CutterTest::core()->getAnalysisClassMethods(className);
    QVERIFY(!findMethod(methods, oldName));
    const AnalysisMethodDescription *method = findMethod(methods, newName);
    QVERIFY(method);
    QCOMPARE(method->addr, desc.addr);
}

CUTTER_CORE_TEST_MAIN(TestEditMethodDialog)
#include "test_EditMethodDialog.moc"
