/**
 * @file test_SettingsUpgrade.cpp
 * @brief Migrating custom color themes at startup.
 *
 * Themes are migrated once. On every later start they are already up to date, which is not
 * an error and must not print the "higher version than current" warning.
 */

#include "TestEnv.h"

#include "common/SettingsUpgrade.h"

#include <QRegularExpression>
#include <QTest>

class TestSettingsUpgrade : public QObject
{
    Q_OBJECT

private slots:
    /** @brief Starting again after the themes were migrated prints no version warning. */
    void migratedThemesDontWarnAgain();
};

void TestSettingsUpgrade::migratedThemesDontWarnAgain()
{
    Cutter::migrateThemes();

    QTest::failOnWarning(QRegularExpression("higher version"));
    Cutter::migrateThemes();
}

CUTTER_CORE_TEST_MAIN(TestSettingsUpgrade)
#include "test_SettingsUpgrade.moc"
