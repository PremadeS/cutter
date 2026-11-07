/**
 * @file test_IOModes.cpp
 * @brief Cutter's IO modes: read-only, cache and write.
 *
 * Cutter owns the rules here: cache and write mode are mutually exclusive, cache
 * mode must never touch the file on disk, and committing the cache or editing in
 * write mode must. A mistake either loses the user's edits or silently changes
 * their binary. When the file can't be written, Cutter must say so and keep the user in
 * Cache mode with their patches. Tests run on a temporary copy of the fixture.
 */

#include "Fixtures.h"
#include "TestEnv.h"
#include "UiHelpers.h"
#include "common/IOModesController.h"

#include <QCryptographicHash>
#include <QFile>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {

QByteArray flippedByte(const QByteArray &original)
{
    return QByteArray(1, static_cast<char>(original.at(0) ^ 0xff));
}

QString hexOf(const QByteArray &bytes)
{
    return QString::fromLatin1(bytes.toHex());
}

} // namespace

class TestIOModes : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void cacheModeEditLeavesFileUntouched();
    void commitWriteCacheWritesFile();
    void writeModeEditPersists();
    void modeSwitchesKeepCacheAndWriteExclusive();
    void cacheModeAndPatchesSurviveEmulation();
    void writeModeOnUnwritableFileKeepsCachedPatches();
    void unwritableFileShowsWriteErrors();

private:
    QByteArray fileHash() const;
    QByteArray patchInCacheMode() const;
    bool makeFileUnwritable() const;
    void makeFileWritable() const;
    void restoreFile() const;
    static bool inMode(bool cache, bool write);

    QTemporaryDir tempDir;
    QString filePath;
    QByteArray originalBytes;
    QByteArray originalHash;
    RVA address = RVA_INVALID;
};

void TestIOModes::initTestCase()
{
    CUTTER_REQUIRE_FIXTURE(const QString fixture, CutterTest::Fixtures::helloX64());
    QVERIFY(tempDir.isValid());
    filePath = tempDir.filePath("io-modes-fixture");
    QVERIFY(QFile::copy(fixture, filePath));
    QVERIFY(QFile::setPermissions(filePath, QFile::ReadOwner | QFile::WriteOwner));

    QFile file(filePath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    originalBytes = file.readAll();
    originalHash = QCryptographicHash::hash(originalBytes, QCryptographicHash::Sha256);
    QVERIFY(CutterTest::openFile(filePath));
    address = CutterTest::core()->getOffset();
    QVERIFY(address != RVA_INVALID);
}

void TestIOModes::init()
{
    QVERIFY(inMode(false, false));
    QCOMPARE(fileHash(), originalHash);
}

void TestIOModes::cleanup()
{
    CutterCore *core = CutterTest::core();
    if (core->currentlyDebugging) {
        core->stopDebug();
    }
    core->resetWriteCache();
    core->setWriteMode(false);
    core->setIOCache(false);
    if (fileHash() != originalHash) {
        restoreFile();
    }
}

void TestIOModes::restoreFile() const
{
    QFile file(filePath);
    if (file.open(QIODevice::ReadWrite)) {
        file.write(originalBytes);
    }
}

QByteArray TestIOModes::fileHash() const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256);
}

QByteArray TestIOModes::patchInCacheMode() const
{
    CutterCore *core = CutterTest::core();
    core->setIOCache(true);
    const QByteArray patched = flippedByte(core->ioRead(address, 1));
    core->editBytes(address, hexOf(patched));
    return patched;
}

bool TestIOModes::makeFileUnwritable() const
{
    if (!QFile::setPermissions(filePath, QFile::ReadOwner)) {
        return false;
    }
    QFile probe(filePath);
    return !probe.open(QIODevice::ReadWrite);
}

void TestIOModes::makeFileWritable() const
{
    QFile::setPermissions(filePath, QFile::ReadOwner | QFile::WriteOwner);
}

bool TestIOModes::inMode(bool cache, bool write)
{
    CutterCore *core = CutterTest::core();
    return core->isIOCacheEnabled() == cache && core->isWriteModeEnabled() == write;
}

void TestIOModes::cacheModeEditLeavesFileUntouched()
{
    CutterCore *core = CutterTest::core();
    core->setIOCache(true);
    QVERIFY(inMode(true, false));

    const QByteArray original = core->ioRead(address, 1);
    const QByteArray patched = flippedByte(original);

    core->editBytes(address, hexOf(patched));
    QCOMPARE(core->ioRead(address, 1), patched);

    QCOMPARE(fileHash(), originalHash);
    core->resetWriteCache();
    QCOMPARE(core->ioRead(address, 1), original);
}

void TestIOModes::commitWriteCacheWritesFile()
{
    CutterCore *core = CutterTest::core();
    core->setIOCache(true);

    const QByteArray original = core->ioRead(address, 1);
    core->editBytes(address, hexOf(flippedByte(original)));
    core->commitWriteCache();
    QVERIFY(fileHash() != originalHash);
    QVERIFY(inMode(true, false));
}

void TestIOModes::writeModeEditPersists()
{
    CutterCore *core = CutterTest::core();
    core->setWriteMode(true);
    QVERIFY(inMode(false, true));

    const QByteArray original = core->ioRead(address, 1);
    core->editBytes(address, hexOf(flippedByte(original)));
    QVERIFY(fileHash() != originalHash);
}

void TestIOModes::modeSwitchesKeepCacheAndWriteExclusive()
{
    CutterCore *core = CutterTest::core();
    QSignalSpy modeSpy(core, &CutterCore::ioModeChanged);
    QSignalSpy cacheSpy(core, &CutterCore::ioCacheChanged);
    QSignalSpy writeSpy(core, &CutterCore::writeModeChanged);

    core->setIOCache(true);
    QVERIFY(inMode(true, false));
    QVERIFY(!modeSpy.isEmpty());
    QCOMPARE(cacheSpy.last().at(0).toBool(), true);

    modeSpy.clear();
    core->setWriteMode(true);
    QVERIFY(inMode(false, true));
    QVERIFY(!modeSpy.isEmpty());
    QCOMPARE(writeSpy.last().at(0).toBool(), true);
    QCOMPARE(cacheSpy.last().at(0).toBool(), false);

    modeSpy.clear();
    core->setIOCache(true);
    QVERIFY(inMode(true, false));
    QVERIFY(!modeSpy.isEmpty());
    QCOMPARE(writeSpy.last().at(0).toBool(), false);
    QCOMPARE(cacheSpy.last().at(0).toBool(), true);

    modeSpy.clear();
    core->setWriteMode(false);
    QVERIFY(inMode(false, false));
    QVERIFY(!modeSpy.isEmpty());
    QCOMPARE(cacheSpy.last().at(0).toBool(), false);

    QCOMPARE(fileHash(), originalHash);
}

void TestIOModes::cacheModeAndPatchesSurviveEmulation()
{
    const int emulationStartTimeoutMs = 30000;
    const int patchLength = 1;
    const char *const ioCacheKey = "io.cache";
    CutterCore *core = CutterTest::core();
    core->setIOCache(true);

    const QByteArray patched = flippedByte(core->ioRead(address, patchLength));
    core->editBytes(address, hexOf(patched));

    core->startEmulation();
    QVERIFY(QTest::qWaitFor(
            [core]() { return core->currentlyEmulating && !core->isDebugTaskInProgress(); },
            emulationStartTimeoutMs));
    core->stopDebug();

    QVERIFY(inMode(true, false));
    QCOMPARE(core->getConfigb(ioCacheKey), core->isIOCacheEnabled());
    QCOMPARE(core->ioRead(address, patchLength), patched);
    QCOMPARE(fileHash(), originalHash);
}

void TestIOModes::writeModeOnUnwritableFileKeepsCachedPatches()
{
    CutterCore *core = CutterTest::core();
    const QByteArray patched = patchInCacheMode();
    const auto writable = qScopeGuard([this]() { makeFileWritable(); });
    if (!makeFileUnwritable()) {
        QSKIP("this user can write to read-only files");
    }

    QVERIFY(!core->setWriteMode(true));
    QVERIFY(inMode(true, false));
    QCOMPARE(core->ioRead(address, patched.size()), patched);

    QVERIFY(!core->commitWriteCache());
    QVERIFY(core->hasUncommitedChanges());
    QCOMPARE(core->ioRead(address, patched.size()), patched);
    QCOMPARE(fileHash(), originalHash);
}

void TestIOModes::unwritableFileShowsWriteErrors()
{
    const int expectedErrors = 2;
    CutterCore *core = CutterTest::core();
    const QByteArray patched = patchInCacheMode();
    const auto writable = qScopeGuard([this]() { makeFileWritable(); });
    if (!makeFileUnwritable()) {
        QSKIP("this user can write to read-only files");
    }
    IOModesController controller(nullptr);
    CutterTest::MessageBoxWatcher messageBoxes;

    controller.setIOMode(IOModesController::Mode::WRITE);
    QVERIFY(inMode(true, false));
    QVERIFY(!controller.commitChanges());
    QVERIFY(core->hasUncommitedChanges());
    QCOMPARE(core->ioRead(address, patched.size()), patched);

    const QStringList errors =
            messageBoxes.seenTexts().filter(QStringLiteral("allowed to write"));
    QCOMPARE(errors.size(), expectedErrors);
}

CUTTER_CORE_TEST_MAIN(TestIOModes)
#include "test_IOModes.moc"
