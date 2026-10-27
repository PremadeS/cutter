/**
 * @file test_LayoutManager.cpp
 * @brief Renaming and deleting the user's saved layouts.
 *
 * Layouts are state only Cutter keeps. The manager must hide the built-in layouts,
 * refuse names that are taken or reserved, keep the layout itself when renaming,
 * and delete only after the user confirms.
 */

#include "TestEnv.h"
#include "UiHelpers.h"

#include "common/CutterLayout.h"
#include "dialogs/LayoutManager.h"

#include <QComboBox>
#include <QInputDialog>
#include <QTest>

namespace {

using Layouts = QMap<QString, Cutter::CutterLayout>;

Cutter::CutterLayout layoutWithState(const QByteArray &state)
{
    Cutter::CutterLayout layout;
    layout.state = state;
    return layout;
}

void answerInputDialogs(QObject *context, QStringList answers)
{
    auto *timer = new QTimer(context);
    timer->setInterval(20);
    QObject::connect(timer, &QTimer::timeout, timer, [answers]() mutable {
        auto *dialog = CutterTest::findVisibleTopLevel<QInputDialog>();
        if (!dialog) {
            return;
        }
        if (answers.isEmpty()) {
            dialog->reject();
            return;
        }
        dialog->setTextValue(answers.takeFirst());
        dialog->accept();
    });
    timer->start();
}

void answerQuestion(QObject *context, QMessageBox::StandardButton answer)
{
    CutterTest::onceVisible<QMessageBox>(context, [answer](QMessageBox *box) {
        if (QAbstractButton *button = box->button(answer)) {
            button->click();
        }
    });
}

QStringList listedNames(LayoutManager &manager)
{
    QStringList names;
    auto *selector = manager.findChild<QComboBox *>("layoutSelector");
    for (int i = 0; selector && i < selector->count(); ++i) {
        names << selector->itemText(i);
    }
    return names;
}

bool select(LayoutManager &manager, const QString &name)
{
    auto *selector = manager.findChild<QComboBox *>("layoutSelector");
    if (!selector) {
        return false;
    }
    selector->setCurrentText(name);
    return selector->currentText() == name;
}

} // namespace

class TestLayoutManager : public QObject
{
    Q_OBJECT

private slots:
    void listsOnlyUserLayouts();
    void renameKeepsLayoutUnderNewName();
    void renameRejectsTakenAndBuiltinNames();
    void cancelledRenameKeepsLayout();
    void deleteNeedsConfirmation();
};

void TestLayoutManager::listsOnlyUserLayouts()
{
    Layouts layouts { { Cutter::layoutDefault, {} }, { Cutter::layoutDebug, {} }, { "mine", {} } };
    LayoutManager manager(layouts, nullptr);

    QCOMPARE(listedNames(manager), QStringList { "mine" });
}

void TestLayoutManager::renameKeepsLayoutUnderNewName()
{
    Layouts layouts { { "mine", layoutWithState("mine-state") } };
    LayoutManager manager(layouts, nullptr);
    QVERIFY(select(manager, "mine"));

    QObject context;
    answerInputDialogs(&context, { "renamed" });
    QVERIFY(CutterTest::clickButton(&manager, "renameButton"));

    QVERIFY(!layouts.contains("mine"));
    QVERIFY(layouts.contains("renamed"));
    QCOMPARE(layouts.value("renamed").state, QByteArray("mine-state"));
    QCOMPARE(listedNames(manager), QStringList { "renamed" });
}

void TestLayoutManager::renameRejectsTakenAndBuiltinNames()
{
    Layouts layouts { { Cutter::layoutDefault, layoutWithState("default-state") },
                      { "mine", layoutWithState("mine-state") },
                      { "other", layoutWithState("other-state") } };
    LayoutManager manager(layouts, nullptr);
    QVERIFY(select(manager, "mine"));

    CutterTest::MessageBoxWatcher warnings;
    QObject context;
    answerInputDialogs(&context, { "other", Cutter::layoutDefault, "fresh" });
    QVERIFY(CutterTest::clickButton(&manager, "renameButton"));

    QCOMPARE(warnings.seenTexts().size(), 2);
    QCOMPARE(layouts.value("other").state, QByteArray("other-state"));
    QCOMPARE(layouts.value(Cutter::layoutDefault).state, QByteArray("default-state"));
    QCOMPARE(layouts.value("fresh").state, QByteArray("mine-state"));
    QVERIFY(!layouts.contains("mine"));
}

void TestLayoutManager::cancelledRenameKeepsLayout()
{
    Layouts layouts { { "mine", layoutWithState("mine-state") } };
    LayoutManager manager(layouts, nullptr);
    QVERIFY(select(manager, "mine"));

    QObject context;
    answerInputDialogs(&context, {});
    QVERIFY(CutterTest::clickButton(&manager, "renameButton"));

    QCOMPARE(layouts.keys(), QStringList { "mine" });
}

void TestLayoutManager::deleteNeedsConfirmation()
{
    Layouts layouts { { "mine", {} } };
    LayoutManager manager(layouts, nullptr);
    QVERIFY(select(manager, "mine"));

    {
        QObject context;
        answerQuestion(&context, QMessageBox::No);
        QVERIFY(CutterTest::clickButton(&manager, "deleteButton"));
    }
    QVERIFY(layouts.contains("mine"));

    {
        QObject context;
        answerQuestion(&context, QMessageBox::Yes);
        QVERIFY(CutterTest::clickButton(&manager, "deleteButton"));
    }
    QVERIFY(!layouts.contains("mine"));
    QVERIFY(listedNames(manager).isEmpty());
}

CUTTER_CORE_TEST_MAIN(TestLayoutManager)
#include "test_LayoutManager.moc"
