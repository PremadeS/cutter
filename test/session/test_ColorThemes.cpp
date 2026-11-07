/**
 * @file test_ColorThemes.cpp
 * @brief Importing a color theme accepts Rizin's own theme files and refuses anything else.
 *
 * Preferences → Appearance → Import only accepted files written exactly the way Cutter exports
 * them, so every theme that ships with Rizin was refused: they all start with a comment, and most
 * set a background color or text attributes. A theme is a Rizin script that runs every time it is
 * loaded, so the check must still refuse a file that would run anything other than color commands.
 *
 * Only isFileTheme is called: importTheme copies into Rizin's real home folder, which the tests
 * don't isolate.
 */

#include "TestEnv.h"
#include "common/ColorThemeWorker.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {

// Rizin themes that use more than color commands, so importing them is refused on purpose:
// `basic` sets SDB keys (`k`), `neon_city` defines aliases (`$name=`), which can hold commands.
const QStringList rizinThemesWithOtherCommands = { "basic", "neon_city" };

} // namespace

class TestColorThemes : public QObject
{
    Q_OBJECT

private slots:
    void rizinThemesAreAccepted_data();
    void rizinThemesAreAccepted();
    void themeLinesAreAccepted_data();
    void themeLinesAreAccepted();
    void otherCommandsAreRefused_data();
    void otherCommandsAreRefused();

private:
    bool isTheme(const QByteArray &content);

    QTemporaryDir dir;
};

bool TestColorThemes::isTheme(const QByteArray &content)
{
    const QString path = QDir(dir.path()).filePath("theme");
    QFile file(path);
    if (!file.open(QFile::WriteOnly | QFile::Truncate)) {
        qWarning("cannot write %s", qPrintable(path));
        return false;
    }
    file.write(content);
    file.close();

    bool ok = false;
    const bool result = ThemeWorker().isFileTheme(path, &ok);
    if (!ok) {
        qWarning("cannot read %s", qPrintable(path));
    }
    return ok && result;
}

void TestColorThemes::rizinThemesAreAccepted_data()
{
    QTest::addColumn<QString>("path");

    const QString themesPath = ThemeWorker().getStandardThemesPath();
    if (themesPath.isEmpty()) {
        QTest::addRow("not installed") << QString();
        return;
    }
    const QDir themes(themesPath);
    for (const QString &name : themes.entryList(QDir::Files)) {
        if (!rizinThemesWithOtherCommands.contains(name)) {
            QTest::addRow("%s", qPrintable(name)) << themes.filePath(name);
        }
    }
}

void TestColorThemes::rizinThemesAreAccepted()
{
    QFETCH(const QString, path);
    if (path.isEmpty()) {
        QSKIP("Rizin's standard themes are not installed");
    }

    bool ok = false;
    QVERIFY(ThemeWorker().isFileTheme(path, &ok));
    QVERIFY(ok);
}

void TestColorThemes::themeLinesAreAccepted_data()
{
    QTest::addColumn<QByteArray>("content");

    QTest::addRow("exported by Cutter") << QByteArray("ec comment rgb:aabbcc\n"
                                                      "#~gui.background rgb:ff112233\n");
    QTest::addRow("comment lines") << QByteArray("# SPDX-License-Identifier: LGPL-3.0-only\n"
                                                 "\n"
                                                 "ec comment rgb:aabbcc\n");
    QTest::addRow("comment after the color")
            << QByteArray("ec b0x00 rgb:738699             # 00 bytes\n");
    QTest::addRow("background color") << QByteArray("ec widget_bg rgb:888 rgb:000\n");
    QTest::addRow("attributes") << QByteArray("ec fname yellow . bold\n");
    QTest::addRow("reset to defaults first") << QByteArray("ecd\nec comment rgb:aabbcc\n");
    QTest::addRow("Windows line endings") << QByteArray("# theme\r\nec comment rgb:aabbcc\r\n");
}

void TestColorThemes::themeLinesAreAccepted()
{
    QFETCH(const QByteArray, content);
    QVERIFY(dir.isValid());

    QVERIFY(isTheme(content));
}

void TestColorThemes::otherCommandsAreRefused_data()
{
    QTest::addColumn<QByteArray>("content");

    QTest::addRow("shell command") << QByteArray("ec comment rgb:aabbcc\n!echo hi\n");
    QTest::addRow("second command after ;") << QByteArray("ec comment rgb:aabbcc; !echo hi\n");
    QTest::addRow("pipe to the shell") << QByteArray("ec comment rgb:aabbcc | sh\n");
    QTest::addRow("command substitution") << QByteArray("ec comment `!echo hi`\n");
    QTest::addRow("output to a file") << QByteArray("ec comment rgb:aabbcc > /tmp/out\n");
    QTest::addRow("alias") << QByteArray("$c=!echo hi\nec comment rgb:aabbcc\n");
    QTest::addRow("write to the binary") << QByteArray("ec comment rgb:aabbcc\nwx 90\n");
    QTest::addRow("empty file") << QByteArray();
    QTest::addRow("only comments") << QByteArray("# not a theme\n");
}

void TestColorThemes::otherCommandsAreRefused()
{
    QFETCH(const QByteArray, content);
    QVERIFY(dir.isValid());

    QVERIFY(!isTheme(content));
}

CUTTER_CORE_TEST_MAIN(TestColorThemes)
#include "test_ColorThemes.moc"
