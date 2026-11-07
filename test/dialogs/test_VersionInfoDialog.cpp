/**
 * @file test_VersionInfoDialog.cpp
 * @brief Right-clicking in the Version info dialog (Dashboard → Version info).
 *
 * The two tables have their own Copy / Select All menu. A right-click anywhere else in the
 * dialog, such as on a table's title, must not open a menu the dialog never created.
 */

#include "TestEnv.h"

#include "dialogs/VersionInfoDialog.h"

#include <QContextMenuEvent>
#include <QLabel>
#include <QPointer>
#include <QTest>

class TestVersionInfoDialog : public QObject
{
    Q_OBJECT

private slots:
    /** @brief Right-clicking a table's title leaves the dialog open. */
    void rightClickOutsideTheTablesKeepsTheDialogOpen();
};

void TestVersionInfoDialog::rightClickOutsideTheTablesKeepsTheDialogOpen()
{
    const QPointer<VersionInfoDialog> dialog = new VersionInfoDialog();
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
    QVERIFY(QTest::qWaitForWindowExposed(dialog));

    auto *title = dialog->findChild<QLabel *>("leftLabel");
    QVERIFY(title);
    const QPoint inside = title->rect().center();
    QContextMenuEvent rightClick(QContextMenuEvent::Mouse, inside, title->mapToGlobal(inside));
    QCoreApplication::sendEvent(title, &rightClick);

    QVERIFY(!dialog.isNull() && dialog->isVisible());
    dialog->close();
}

CUTTER_CORE_TEST_MAIN(TestVersionInfoDialog)
#include "test_VersionInfoDialog.moc"
