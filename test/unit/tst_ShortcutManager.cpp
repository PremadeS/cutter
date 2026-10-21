#include "shortcuts/ShortcutManager.h"

#include <QtTest/QtTest>
#include <QAction>
#include <QShortcut>
#include <QWidget>

class TestShortcutManager : public QObject
{
    Q_OBJECT

private slots:
    void testMatchesKeySequence();
    void testMakeActionAndSetupAction();
    void testMakeQShortcut();
};

void TestShortcutManager::testMatchesKeySequence()
{
    ShortcutManager *sm = ShortcutManager::getInstance();
    auto allShortcuts = sm->getAllShortcuts();

    if (allShortcuts.isEmpty()) {
        QSKIP("No default shortcuts registered to test matching");
    }

    // Find the first registered shortcut with key sequences
    QString testId;
    QKeySequence validSeq;
    for (auto it = allShortcuts.cbegin(); it != allShortcuts.cend(); ++it) {
        if (!it.value().keySequences.isEmpty()) {
            testId = it.key();
            validSeq = it.value().keySequences.first();
            break;
        }
    }

    if (!testId.isEmpty()) {
        QVERIFY(sm->matchesKeySequence(testId, validSeq));
        QVERIFY(!sm->matchesKeySequence(testId, QKeySequence("Ctrl+Shift+Alt+Win+Z")));
    }
}

void TestShortcutManager::testMakeActionAndSetupAction()
{
    ShortcutManager *sm = ShortcutManager::getInstance();
    auto allShortcuts = sm->getAllShortcuts();

    if (allShortcuts.isEmpty()) {
        QSKIP("No default shortcuts available to construct QAction");
    }

    const QString validId = allShortcuts.keys().first();

    // Test setupAction
    QAction action(nullptr);
    sm->setupAction(action, validId);
    QCOMPARE(action.shortcuts(), sm->getKeySequences(validId));

    // Test makeAction
    QAction *createdAction = sm->makeAction(validId, this);
    QVERIFY(createdAction != nullptr);
    QCOMPARE(createdAction->shortcuts(), sm->getKeySequences(validId));

    delete createdAction;
}

void TestShortcutManager::testMakeQShortcut()
{
    ShortcutManager *sm = ShortcutManager::getInstance();
    QWidget parentWidget;

    auto allShortcuts = sm->getAllShortcuts();
    if (allShortcuts.isEmpty()) {
        QSKIP("No default shortcuts available to construct QShortcut");
    }

    const QString validId = allShortcuts.keys().first();
    QShortcut *qshortcut = sm->makeQShortcut(validId, &parentWidget);

    QVERIFY(qshortcut != nullptr);
    delete qshortcut;
}

QTEST_MAIN(TestShortcutManager)
#include "tst_ShortcutManager.moc"
