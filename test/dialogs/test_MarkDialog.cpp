/**
 * @file test_MarkDialog.cpp
 * @brief Creating and editing marks (named address ranges).
 *
 * MarkDialog validates the range and name before touching the core, proposes a
 * default name for new marks, and implements rename as delete-then-add. A gap in
 * that logic either stores an invalid range or loses the user's mark on rename.
 */

#include "Fixtures.h"
#include "TestEnv.h"
#include "UiHelpers.h"

#include "dialogs/MarkDialog.h"

#include <QTest>
#include <QTextEdit>

namespace {

constexpr RVA markLength = 0x10;

const MarkDescription *findMark(const QList<MarkDescription> &marks, const QString &name)
{
    for (const MarkDescription &mark : marks) {
        if (mark.name == name) {
            return &mark;
        }
    }
    return nullptr;
}

bool setRange(MarkDialog &dialog, RVA from, RVA to)
{
    return CutterTest::setLineEditText(&dialog, "startAddressEdit", rzAddressString(from))
            && CutterTest::setLineEditText(&dialog, "endAddressEdit", rzAddressString(to));
}

QString fieldText(MarkDialog &dialog, const char *objectName)
{
    auto *edit = dialog.findChild<QLineEdit *>(objectName);
    return edit ? edit->text() : QString();
}

} // namespace

class TestMarkDialog : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void newMarkUsesProposedName();
    void startAfterEndIsRejected();
    void emptyNameIsRejected();
    void renameReplacesMark();

private:
    RVA from = RVA_INVALID;
    RVA to = RVA_INVALID;
    const QString name = QStringLiteral("cutter_test_mark");
    const QString renamed = QStringLiteral("cutter_test_mark_renamed");
};

void TestMarkDialog::initTestCase()
{
    const QString path = CutterTest::fixturePath(CutterTest::Fixtures::helloX64());
    if (path.isEmpty()) {
        QSKIP("rizin-testbins not available, set CUTTER_TESTBINS_DIR");
    }
    QVERIFY(CutterTest::openFile(path));
    from = CutterTest::core()->getOffset();
    QVERIFY(from != RVA_INVALID);
    to = from + markLength;
}

void TestMarkDialog::init()
{
    CutterCore *core = CutterTest::core();
    const QList<MarkDescription> marks = core->getMarks();
    for (const MarkDescription &mark : marks) {
        core->delMark(mark.name);
    }
    QVERIFY(core->getMarks().isEmpty());
}

void TestMarkDialog::newMarkUsesProposedName()
{
    MarkDialog dialog(from, to);
    const QString proposed = fieldText(dialog, "nameEdit");
    QVERIFY(!proposed.isEmpty());

    QVERIFY(CutterTest::clickStandardButton(&dialog, QDialogButtonBox::Ok));
    QCOMPARE(dialog.result(), int(QDialog::Accepted));

    const QList<MarkDescription> marks = CutterTest::core()->getMarks();
    const MarkDescription *mark = findMark(marks, proposed);
    QVERIFY(mark);
    QCOMPARE(mark->from, from);
    QCOMPARE(mark->to, to);
}

void TestMarkDialog::startAfterEndIsRejected()
{
    CutterTest::MessageBoxWatcher messageBoxes;

    MarkDialog dialog(from, to);
    QVERIFY(setRange(dialog, to, from));
    QVERIFY(CutterTest::setLineEditText(&dialog, "nameEdit", name));
    QVERIFY(CutterTest::clickStandardButton(&dialog, QDialogButtonBox::Ok));

    QCOMPARE(messageBoxes.seenTexts().size(), 1);
    QVERIFY(dialog.result() != QDialog::Accepted);
    QVERIFY(CutterTest::core()->getMarks().isEmpty());
}

void TestMarkDialog::emptyNameIsRejected()
{
    CutterTest::MessageBoxWatcher messageBoxes;

    MarkDialog dialog(from, to);
    QVERIFY(CutterTest::setLineEditText(&dialog, "nameEdit", QString()));
    QVERIFY(CutterTest::clickStandardButton(&dialog, QDialogButtonBox::Ok));

    QCOMPARE(messageBoxes.seenTexts().size(), 1);
    QVERIFY(dialog.result() != QDialog::Accepted);
    QVERIFY(CutterTest::core()->getMarks().isEmpty());
}

void TestMarkDialog::renameReplacesMark()
{
    const QString comment = QStringLiteral("kept across rename");
    CutterTest::core()->addMark(from, to, name, comment);

    MarkDialog dialog(from, to, nullptr, name);
    QCOMPARE(fieldText(dialog, "nameEdit"), name);
    auto *commentEdit = dialog.findChild<QTextEdit *>("commentEdit");
    QVERIFY(commentEdit);
    QCOMPARE(commentEdit->toPlainText(), comment);

    QVERIFY(CutterTest::setLineEditText(&dialog, "nameEdit", renamed));
    QVERIFY(CutterTest::clickStandardButton(&dialog, QDialogButtonBox::Ok));

    const QList<MarkDescription> marks = CutterTest::core()->getMarks();
    QVERIFY(!findMark(marks, name));
    const MarkDescription *mark = findMark(marks, renamed);
    QVERIFY(mark);
    QCOMPARE(mark->from, from);
    QCOMPARE(mark->to, to);
    QCOMPARE(mark->comment, comment);
}

CUTTER_CORE_TEST_MAIN(TestMarkDialog)
#include "test_MarkDialog.moc"
