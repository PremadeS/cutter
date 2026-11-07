/**
 * @file test_DataGetters.cpp
 * @brief Getters that copy rizin's data into Cutter's descriptions.
 *
 * The list docks and dialogs show exactly what these getters return. A field
 * copied from the wrong place shows the user wrong data with no error.
 */

#include "Fixtures.h"
#include "TestEnv.h"

#include <QSet>
#include <QTest>

namespace {

using XrefKey = QPair<RVA, RVA>;

QSet<XrefKey> xrefKeys(const QList<XrefDescription> &xrefs)
{
    QSet<XrefKey> keys;
    for (const XrefDescription &xref : xrefs) {
        keys.insert({ xref.from, xref.to });
    }
    return keys;
}

} // namespace

class TestDataGetters : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    /** @brief The Flags dock's Real Name column shows rizin's real name, not the name again. */
    void flagsKeepTheirRealName();
    /** @brief X-Refs from a whole function include the references of every instruction in it. */
    void wholeFunctionXrefsFromCoverEveryInstruction();
};

void TestDataGetters::initTestCase()
{
    CUTTER_REQUIRE_FIXTURE(const QString path, CutterTest::Fixtures::helloX64());
    QVERIFY(CutterTest::openFile(path));
    CutterTest::analyze();
}

void TestDataGetters::flagsKeepTheirRealName()
{
    CutterCore *core = CutterTest::core();
    const QList<FlagDescription> flags = core->getAllFlags();
    QVERIFY(!flags.isEmpty());

    bool sawDistinctRealName = false;
    RzCoreLocked locked(core);
    for (const FlagDescription &flag : flags) {
        const RzFlagItem *item = rz_flag_get(locked->flags, flag.name.toUtf8().constData());
        QVERIFY2(item, qPrintable(flag.name));
        const QString realName = QString::fromUtf8(item->realname ? item->realname : item->name);
        QCOMPARE(flag.realname, realName);
        sawDistinctRealName = sawDistinctRealName || realName != flag.name;
    }
    QVERIFY2(sawDistinctRealName, "fixture needs a flag whose real name differs from its name");
}

void TestDataGetters::wholeFunctionXrefsFromCoverEveryInstruction()
{
    CutterCore *core = CutterTest::core();
    const char *const functionName = "main";

    RVA start = RVA_INVALID;
    QList<QPair<RVA, RVA>> blocks;
    {
        RzCoreLocked locked(core);
        const RzAnalysisFunction *function =
                rz_analysis_get_function_byname(locked->analysis, functionName);
        QVERIFY2(function, functionName);
        start = function->addr;
        void **it;
        rz_pvector_foreach (function->bbs, it) {
            const auto *block = static_cast<const RzAnalysisBlock *>(*it);
            blocks.append({ block->addr, block->addr + block->size });
        }
    }

    QSet<XrefKey> expected;
    for (const auto &[blockStart, blockEnd] : std::as_const(blocks)) {
        for (RVA address = blockStart; address < blockEnd; ++address) {
            expected.unite(xrefKeys(core->getXRefs(address, false, false)));
        }
    }
    QVERIFY2(!expected.isEmpty(), "fixture's main needs references from its instructions");

    const QSet<XrefKey> actual = xrefKeys(core->getXRefs(start, false, true));
    for (const XrefKey &key : std::as_const(expected)) {
        QVERIFY2(actual.contains(key),
                 qPrintable(QStringLiteral("missing xref %1 -> %2")
                                    .arg(rzAddressString(key.first), rzAddressString(key.second))));
    }
}

CUTTER_CORE_TEST_MAIN(TestDataGetters)
#include "test_DataGetters.moc"
