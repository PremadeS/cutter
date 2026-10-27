#ifndef CUTTER_TEST_FIXTURES_H
#define CUTTER_TEST_FIXTURES_H

#include <QString>

namespace CutterTest::Fixtures {

inline QString helloX64()
{
    return QStringLiteral("elf/analysis/hello-linux-x86_64");
}

inline QString crackmeX86()
{
    return QStringLiteral("elf/ioli/crackme0x00");
}

inline QString coreutilsLs()
{
    return QStringLiteral("elf/ls");
}

} // namespace CutterTest::Fixtures

#endif // CUTTER_TEST_FIXTURES_H
