#include "CutterTest.h"

#include "common/Configuration.h"
#include "core/Cutter.h"

CutterTest::CutterTest(bool initCore, bool initConfig, QObject *parent)
    : initCore(initCore), initConfig(initConfig), QObject(parent)
{
}

void CutterTest::initTestCase()
{
    Q_INIT_RESOURCE(resources);
    Q_INIT_RESOURCE(dark);
    Q_INIT_RESOURCE(light);
    Q_INIT_RESOURCE(native);
    Q_INIT_RESOURCE(midnight);

    if (initCore && !Core()) {
        new CutterCore(qApp);
        Core()->initialize();
    }

    if (initCore || initConfig) {
        Config()->loadInitial();
    }
}

void CutterTest::cleanupTestCase() {}

void CutterTest::init() {}

void CutterTest::cleanup() {}