/**
 * @file test_ShortcutManager.cpp
 * @brief Cutter's default shortcut table and how shortcuts are matched.
 *
 * Shortcuts built from platform key bindings can end up with no key at all on some OS.
 * Widgets that handle keys themselves rely on matchesKeySequence checking every key of
 * a shortcut. Whether two shortcuts clash depends on where they are active, which only
 * the live widgets know, so that is checked in workflows/test_Shortcuts.cpp.
 */

#include "shortcuts/ShortcutManager.h"

#include <QtTest/QtTest>

class TestShortcutManager : public QObject
{
    Q_OBJECT

private slots:
    void everyShortcutHasKeyOnThisPlatform();
    void matchesEveryKeyOfShortcut();
};

void TestShortcutManager::everyShortcutHasKeyOnThisPlatform()
{
    const QHash<QString, Shortcut> shortcuts = Shortcuts()->getAllShortcuts();
    QStringList missing;

    for (auto it = shortcuts.cbegin(); it != shortcuts.cend(); ++it) {
        bool hasKey = false;
        for (const QKeySequence &sequence : it.value().keySequences) {
            hasKey = hasKey || !sequence.isEmpty();
        }
        if (!hasKey) {
            missing << it.key();
        }
    }

    missing.sort();
    QVERIFY2(missing.isEmpty(), qPrintable("no key: " + missing.join(", ")));
}

void TestShortcutManager::matchesEveryKeyOfShortcut()
{
    const QList<QKeySequence> zoomIn = Shortcuts()->getKeySequences("General.zoomIn");
    QVERIFY(zoomIn.size() > 1);

    for (const QKeySequence &sequence : zoomIn) {
        QVERIFY(Shortcuts()->matchesKeySequence("General.zoomIn", sequence));
        QVERIFY(!Shortcuts()->matchesKeySequence("General.zoomOut", sequence));
    }
    QVERIFY(!Shortcuts()->matchesKeySequence("General.zoomIn", QKeySequence()));
}

QTEST_MAIN(TestShortcutManager)
#include "test_ShortcutManager.moc"
