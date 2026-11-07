/**
 * @file test_Appearance.cpp
 * @brief The Native interface theme follows the system's light or dark appearance.
 *
 * With the Native theme, switching macOS (or another desktop) between light and dark
 * while Cutter runs used to leave the old color theme in place: light text colors on a
 * dark window, or the reverse, until a restart. The system's change is simulated by
 * emitting the style hints' colorSchemeChanged signal, which is what the platform sends.
 */

#include "TestEnv.h"
#include "common/Configuration.h"

#include <QGuiApplication>
#include <QScopeGuard>
#include <QStyleHints>
#include <QTest>

#include <algorithm>
#include <iterator>

class TestAppearance : public QObject
{
    Q_OBJECT

private slots:
    void nativeThemeFollowsSystemColorScheme_data();
    void nativeThemeFollowsSystemColorScheme();
};

void TestAppearance::nativeThemeFollowsSystemColorScheme_data()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    QTest::addColumn<Qt::ColorScheme>("startScheme");
    QTest::addColumn<Qt::ColorScheme>("newScheme");

    QTest::addRow("light to dark") << Qt::ColorScheme::Light << Qt::ColorScheme::Dark;
    QTest::addRow("dark to light") << Qt::ColorScheme::Dark << Qt::ColorScheme::Light;
#endif
}

void TestAppearance::nativeThemeFollowsSystemColorScheme()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    QFETCH(const Qt::ColorScheme, startScheme);
    QFETCH(const Qt::ColorScheme, newScheme);
    QStyleHints *hints = QGuiApplication::styleHints();
    const Qt::ColorScheme systemScheme = hints->colorScheme();
    const auto restore =
            qScopeGuard([hints, systemScheme]() { emit hints->colorSchemeChanged(systemScheme); });

    const QList<CutterInterfaceTheme> &themes = Configuration::cutterInterfaceThemesList();
    const auto native = std::find_if(themes.cbegin(), themes.cend(), [](const auto &theme) {
        return theme.name == QLatin1String("Native");
    });
    QVERIFY(native != themes.cend());
    emit hints->colorSchemeChanged(startScheme);
    Config()->setInterfaceTheme(static_cast<int>(std::distance(themes.cbegin(), native)));

    emit hints->colorSchemeChanged(newScheme);

    const bool dark = newScheme == Qt::ColorScheme::Dark;
    QCOMPARE(Config()->windowColorIsDark(), dark);
    const int windowDarkness = dark ? DarkFlag : LightFlag;
    QVERIFY2(Config()->colorThemeDarkness(Config()->getColorTheme()) & windowDarkness,
             qPrintable("color theme " + Config()->getColorTheme() + " doesn't fit the window"));
#else
    QSKIP("needs Qt 6.5 to know the system color scheme");
#endif
}

CUTTER_CORE_TEST_MAIN(TestAppearance)
#include "test_Appearance.moc"
