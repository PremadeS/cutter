/**
 * @file test_HexdumpRangeDialog.cpp
 * @brief Choosing an address range by end address or by length.
 *
 * The dialog keeps the end and length fields in sync, turns the user's exclusive
 * end into an inclusive last address, and only enables OK for a usable range.
 * Mistakes here select one byte too many or too few, or let an invalid or
 * overflowing range through to every action that uses it.
 */

#include "TestEnv.h"
#include "UiHelpers.h"

#include "dialogs/HexdumpRangeDialog.h"

#include <QRadioButton>
#include <QTest>

namespace {

bool typeInto(QWidget *dialog, const char *objectName, const QString &text)
{
    auto *edit = dialog->findChild<QLineEdit *>(objectName);
    if (!edit) {
        return false;
    }
    edit->clear();
    QTest::keyClicks(edit, text);
    return true;
}

QString fieldText(QWidget *dialog, const char *objectName)
{
    auto *edit = dialog->findChild<QLineEdit *>(objectName);
    return edit ? edit->text() : QString();
}

bool okEnabled(QWidget *dialog)
{
    auto *buttons = dialog->findChild<QDialogButtonBox *>();
    QPushButton *ok = buttons ? buttons->button(QDialogButtonBox::Ok) : nullptr;
    return ok && ok->isEnabled();
}

bool useLengthMode(QWidget *dialog)
{
    auto *radio = dialog->findChild<QRadioButton *>("lengthRadioButton");
    if (!radio) {
        return false;
    }
    radio->click();
    return radio->isChecked();
}

} // namespace

class TestHexdumpRangeDialog : public QObject
{
    Q_OBJECT

private slots:
    void endModeComputesLengthAndLastAddress();
    void lengthModeComputesEnd();
    void endBeforeStartIsInvalid();
    void emptyRangeNeedsAllowEmpty();
    void lengthOverflowIsInvalid();
};

void TestHexdumpRangeDialog::endModeComputesLengthAndLastAddress()
{
    HexdumpRangeDialog dialog;
    dialog.show();
    QVERIFY(typeInto(&dialog, "startAddressLineEdit", "0x1000"));
    QVERIFY(typeInto(&dialog, "endAddressLineEdit", "0x1010"));

    QVERIFY(okEnabled(&dialog));
    QCOMPARE(fieldText(&dialog, "lengthLineEdit"), QString("0x10"));
    QCOMPARE(dialog.getStartAddress(), RVA(0x1000));
    QCOMPARE(dialog.getEndAddress(), RVA(0x100f));
    QVERIFY(!dialog.empty());
}

void TestHexdumpRangeDialog::lengthModeComputesEnd()
{
    HexdumpRangeDialog dialog;
    dialog.show();
    QVERIFY(useLengthMode(&dialog));
    QVERIFY(typeInto(&dialog, "startAddressLineEdit", "0x1000"));
    QVERIFY(typeInto(&dialog, "lengthLineEdit", "0x10"));

    QVERIFY(okEnabled(&dialog));
    QCOMPARE(fieldText(&dialog, "endAddressLineEdit"), QString("0x1010"));
    QCOMPARE(dialog.getEndAddress(), RVA(0x100f));
    QVERIFY(!dialog.empty());
}

void TestHexdumpRangeDialog::endBeforeStartIsInvalid()
{
    HexdumpRangeDialog dialog;
    dialog.show();
    QVERIFY(typeInto(&dialog, "startAddressLineEdit", "0x1010"));
    QVERIFY(typeInto(&dialog, "endAddressLineEdit", "0x1000"));

    QVERIFY(!okEnabled(&dialog));
    QCOMPARE(fieldText(&dialog, "lengthLineEdit"), QString("Invalid"));
}

void TestHexdumpRangeDialog::emptyRangeNeedsAllowEmpty()
{
    HexdumpRangeDialog strict(nullptr, false);
    strict.show();
    QVERIFY(typeInto(&strict, "startAddressLineEdit", "0x1000"));
    QVERIFY(typeInto(&strict, "endAddressLineEdit", "0x1000"));
    QVERIFY(!okEnabled(&strict));

    HexdumpRangeDialog lenient(nullptr, true);
    lenient.show();
    QVERIFY(typeInto(&lenient, "startAddressLineEdit", "0x1000"));
    QVERIFY(typeInto(&lenient, "endAddressLineEdit", "0x1000"));
    QVERIFY(okEnabled(&lenient));
    QVERIFY(lenient.empty());
}

void TestHexdumpRangeDialog::lengthOverflowIsInvalid()
{
    HexdumpRangeDialog dialog;
    dialog.show();
    QVERIFY(useLengthMode(&dialog));
    QVERIFY(typeInto(&dialog, "startAddressLineEdit", "0xffffffffffffff00"));
    QVERIFY(typeInto(&dialog, "lengthLineEdit", "0x200"));

    QVERIFY(!okEnabled(&dialog));
    QCOMPARE(fieldText(&dialog, "endAddressLineEdit"), QString("Invalid"));
}

CUTTER_CORE_TEST_MAIN(TestHexdumpRangeDialog)
#include "test_HexdumpRangeDialog.moc"
