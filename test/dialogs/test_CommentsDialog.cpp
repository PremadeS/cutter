/**
 * @file test_CommentsDialog.cpp
 * @brief Adding, editing and removing comments through the comment dialog.
 *
 * CommentsDialog::addOrEditComment decides what the user's input means: it prefills
 * the existing comment, an empty comment deletes it, and cancel changes nothing.
 * Every "Add comment" action in Cutter goes through it.
 */

#include "Fixtures.h"
#include "TestEnv.h"
#include "UiHelpers.h"

#include "dialogs/CommentsDialog.h"

#include <QTest>

namespace {

struct DialogResult
{
    bool shown = false;
    QString prefill;
};

DialogResult runDialog(QObject *context, RVA address, const QString &input, bool accept)
{
    DialogResult result;
    CutterTest::onceVisible<CommentsDialog>(context, [&](CommentsDialog *dialog) {
        result.shown = true;
        result.prefill = dialog->getComment();
        dialog->setComment(input);
        if (accept) {
            dialog->accept();
        } else {
            dialog->reject();
        }
    });
    CommentsDialog::addOrEditComment(address, nullptr);
    return result;
}

} // namespace

class TestCommentsDialog : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void acceptAddsComment();
    void prefillsAndEditsExistingComment();
    void emptyCommentDeletesExisting();
    void cancelLeavesCommentUnchanged();

private:
    RVA address = RVA_INVALID;
};

void TestCommentsDialog::initTestCase()
{
    const QString path = CutterTest::fixturePath(CutterTest::Fixtures::helloX64());
    if (path.isEmpty()) {
        QSKIP("rizin-testbins not available, set CUTTER_TESTBINS_DIR");
    }
    QVERIFY(CutterTest::openFile(path));

    address = CutterTest::addressWithoutComment(CutterTest::core()->getOffset());
    QVERIFY2(address != RVA_INVALID, "no address without a comment near the current seek");
}

void TestCommentsDialog::init()
{
    CutterTest::core()->delComment(address);
}

void TestCommentsDialog::acceptAddsComment()
{
    QObject context;
    const DialogResult result = runDialog(&context, address, "added", true);

    QVERIFY(result.shown);
    QVERIFY(result.prefill.isEmpty());
    QCOMPARE(CutterTest::core()->getCommentAt(address), QString("added"));
}

void TestCommentsDialog::prefillsAndEditsExistingComment()
{
    CutterTest::core()->setComment(address, "existing");

    QObject context;
    const DialogResult result = runDialog(&context, address, "edited", true);

    QVERIFY(result.shown);
    QCOMPARE(result.prefill, QString("existing"));
    QCOMPARE(CutterTest::core()->getCommentAt(address), QString("edited"));
}

void TestCommentsDialog::emptyCommentDeletesExisting()
{
    CutterTest::core()->setComment(address, "existing");

    QObject context;
    const DialogResult result = runDialog(&context, address, QString(), true);

    QVERIFY(result.shown);
    QVERIFY(CutterTest::core()->getCommentAt(address).isEmpty());
}

void TestCommentsDialog::cancelLeavesCommentUnchanged()
{
    CutterTest::core()->setComment(address, "existing");

    QObject context;
    const DialogResult result = runDialog(&context, address, "discarded", false);

    QVERIFY(result.shown);
    QCOMPARE(CutterTest::core()->getCommentAt(address), QString("existing"));
}

CUTTER_CORE_TEST_MAIN(TestCommentsDialog)
#include "test_CommentsDialog.moc"
