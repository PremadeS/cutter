/**
 * @file test_Config.cpp
 * @brief Resetting the rizin configuration from Preferences.
 *
 * "Reset to Default" must only reset the user's preferences. Settings that come
 * from the opened file (arch, bits, endianness), the IO mode and the values Cutter
 * depends on must survive it, or the disassembly is decoded wrongly, edits stop
 * being cached, or rizin starts prompting on the terminal.
 */

#include "Fixtures.h"
#include "TestEnv.h"

#include <QFile>
#include <QScopeGuard>
#include <QTest>

namespace {

int elfBits(const QString &path)
{
    const QByteArray elfMagic = QByteArrayLiteral("\x7f"
                                                  "ELF");
    const qsizetype classIndex = elfMagic.size();
    const char elfClass32 = 1;
    const char elfClass64 = 2;
    const int bits32 = 32;
    const int bits64 = 64;
    const int unknownBits = 0;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return unknownBits;
    }
    const QByteArray ident = file.read(classIndex + 1);
    if (ident.size() <= classIndex || !ident.startsWith(elfMagic)) {
        return unknownBits;
    }
    const char elfClass = ident.at(classIndex);
    if (elfClass == elfClass32) {
        return bits32;
    }
    if (elfClass == elfClass64) {
        return bits64;
    }
    return unknownBits;
}

} // namespace

class TestConfig : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void resetKeepsTheFileArchitecture();
    void resetKeepsCacheMode();
    void resetKeepsRizinNonInteractive();

private:
    QString path;
};

void TestConfig::initTestCase()
{
    CUTTER_REQUIRE_FIXTURE(const QString fixture, CutterTest::Fixtures::crackmeX86());
    path = fixture;
    QVERIFY(CutterTest::openFile(path));
}

void TestConfig::resetKeepsTheFileArchitecture()
{
    const char *const archKey = "asm.arch";
    const char *const cpuKey = "asm.cpu";
    const char *const bitsKey = "asm.bits";
    const char *const osKey = "asm.os";
    const char *const bigEndianKey = "cfg.bigendian";

    CutterCore *core = CutterTest::core();
    const int bits = core->getConfigi(bitsKey);
    QCOMPARE(bits, elfBits(path));
    const QString arch = core->getConfig(archKey);
    const QString cpu = core->getConfig(cpuKey);
    const QString os = core->getConfig(osKey);
    const bool bigEndian = core->getConfigb(bigEndianKey);

    core->resetConfig();

    QCOMPARE(core->getConfig(archKey), arch);
    QCOMPARE(core->getConfig(cpuKey), cpu);
    QCOMPARE(core->getConfigi(bitsKey), bits);
    QCOMPARE(core->getConfig(osKey), os);
    QCOMPARE(core->getConfigb(bigEndianKey), bigEndian);
}

void TestConfig::resetKeepsCacheMode()
{
    CutterCore *core = CutterTest::core();
    core->setIOCache(true);
    const auto leaveCacheMode = qScopeGuard([core]() {
        core->resetWriteCache();
        core->setIOCache(false);
    });

    core->resetConfig();
    QVERIFY(core->isIOCacheEnabled());
    const char *const ioCacheKey = "io.cache";
    QCOMPARE(core->getConfigb(ioCacheKey), core->isIOCacheEnabled());

    const int editedLength = 1;
    const char allBits = static_cast<char>(0xff);
    const RVA address = core->getOffset();
    const QByteArray original = core->ioRead(address, editedLength);
    QCOMPARE(original.size(), editedLength);
    const QByteArray patched =
            QByteArray(editedLength, static_cast<char>(original.front() ^ allBits));
    core->editBytes(address, QString::fromLatin1(patched.toHex()));
    QCOMPARE(core->ioRead(address, editedLength), patched);
}

void TestConfig::resetKeepsRizinNonInteractive()
{
    const char *const interactiveKey = "scr.interactive";
    CutterCore *core = CutterTest::core();
    QVERIFY(!core->getConfigb(interactiveKey));

    core->resetConfig();

    QVERIFY(!core->getConfigb(interactiveKey));
}

CUTTER_CORE_TEST_MAIN(TestConfig)
#include "test_Config.moc"
