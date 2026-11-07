/**
 * @file test_WelcomeDialog.cpp
 * @brief Choosing a language in the Welcome dialog shown on Cutter's first start.
 *
 * Picking a language tells the user, once, that it applies after a restart.
 */

#include "TestEnv.h"
#include "UiHelpers.h"

#include "common/Configuration.h"
#include "dialogs/WelcomeDialog.h"

#include <QComboBox>
#include <QLocale>
#include <QScopeGuard>
#include <QSignalBlocker>
#include <QTest>

class TestWelcomeDialog : public QObject
{
    Q_OBJECT

private slots:
    /** @brief Changing the language shows the restart notice once. */
    void languageChangeIsAnnouncedOnce();
};

void TestWelcomeDialog::languageChangeIsAnnouncedOnce()
{
    const QLocale previousLocale = Config()->getCurrLocale();
    const auto restoreLocale =
            qScopeGuard([previousLocale]() { Config()->setLocale(previousLocale); });

    WelcomeDialog dialog;
    auto *languages = dialog.findChild<QComboBox *>("languageComboBox");
    QVERIFY(languages);
    {
        const QSignalBlocker quiet(languages);
        for (const QLocale::Language language : { QLocale::French, QLocale::German }) {
            const QLocale locale(language);
            languages->addItem(locale.nativeLanguageName(), locale);
        }
    }
    const int last = languages->count() - 1;
    const int otherLanguage = languages->currentIndex() == last ? last - 1 : last;

    CutterTest::MessageBoxWatcher messageBoxes;
    languages->setCurrentIndex(otherLanguage);

    QCOMPARE(messageBoxes.seenTexts().size(), 1);
}

CUTTER_CORE_TEST_MAIN(TestWelcomeDialog)
#include "test_WelcomeDialog.moc"
