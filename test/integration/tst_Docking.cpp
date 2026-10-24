#include "CutterApplication.h"
#include "CutterDockWidget.h"
#include "core/MainWindow.h"
#include "shortcuts/ShortcutManager.h"

#include <QCoreApplication>
#include <QMoveEvent>
#include <QSignalSpy>
#include <QTest>
#include <QWindow>

class TestDocking : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testDockingPreventedByModifier();
    void testBecameVisibleToUserSignal();
    void testTransientDockWidgetClose();
    void cleanupTestCase();

private:
    MainWindow *mainWindow = nullptr;
};

void TestDocking::initTestCase()
{
    Q_INIT_RESOURCE(resources);

    for (QWidget *widget : QApplication::topLevelWidgets()) {
        if (auto *mw = qobject_cast<MainWindow *>(widget)) {
            mainWindow = mw;
            break;
        }
    }

    QVERIFY2(mainWindow != nullptr, "MainWindow not found among top-level widgets!");

    mainWindow->show();
    QApplication::processEvents();
}

void TestDocking::testDockingPreventedByModifier()
{
    auto *dock = new CutterDockWidget(mainWindow);
    dock->setWindowTitle("Automated Test Dock");
    mainWindow->addDockWidget(Qt::LeftDockWidgetArea, dock);

    // 1. Initial State: Float the widget
    dock->setFloating(true);
    dock->show();
    dock->raise();
    QApplication::processEvents();

    QCOMPARE(dock->isFloating(), true);

    QWindow *win = mainWindow->windowHandle();
    QVERIFY2(win != nullptr, "MainWindow native QWindow handle is null");

    const Qt::KeyboardModifier mod =
            Shortcuts()->convertKeyToModifer(Shortcuts()->getKeySequence("Docking.toggle"));

    const Qt::Key keyToPress = (mod == Qt::ControlModifier) ? Qt::Key_Control
            : (mod == Qt::ShiftModifier)                    ? Qt::Key_Shift
            : (mod == Qt::AltModifier)                      ? Qt::Key_Alt
                                                            : Qt::Key_Meta;

    QMoveEvent moveEvent(QPoint(100, 100), QPoint(0, 0));

    // ------------------------------------------------------------------
    // 2. MOVE WITH MODIFIER ACTIVE (Docking areas must be BLOCKED)
    // ------------------------------------------------------------------
    QTest::keyPress(win, keyToPress, mod);
    QCoreApplication::sendEvent(dock, &moveEvent);

    // CutterDockWidget::event catches QEvent::Move and sets allowedAreas to NoDockWidgetArea
    QCOMPARE(dock->allowedAreas(), Qt::NoDockWidgetArea);
    QCOMPARE(dock->isFloating(), true);

    // ------------------------------------------------------------------
    // 3. MOVE WITHOUT MODIFIER (Docking areas must be RESTORED)
    // ------------------------------------------------------------------
    QTest::keyRelease(win, keyToPress, Qt::NoModifier);
    QCoreApplication::sendEvent(dock, &moveEvent);

    // CutterDockWidget::event restores allowedAreas to AllDockWidgetAreas
    QCOMPARE(dock->allowedAreas(), Qt::AllDockWidgetAreas);

    // Reset floating state now that allowedAreas is restored
    dock->setFloating(false);
    QApplication::processEvents();

    QCOMPARE(dock->isFloating(), false);
    QCOMPARE(mainWindow->dockWidgetArea(dock), Qt::LeftDockWidgetArea);

    dock->deleteLater();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

void TestDocking::testBecameVisibleToUserSignal()
{
    auto *dock = new CutterDockWidget(mainWindow);
    dock->setWindowTitle("Visibility Signal Test Dock");
    QSignalSpy spyVisible(dock, &CutterDockWidget::becameVisibleToUser);

    dock->show();
    QApplication::processEvents();

    const int initialCount = spyVisible.count();

    dock->ignoreVisibilityStatus(true);
    dock->hide();
    dock->show();
    QApplication::processEvents();

    QCOMPARE(spyVisible.count(), initialCount);

    dock->deleteLater();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

void TestDocking::testTransientDockWidgetClose()
{
    auto *dock = new CutterDockWidget(mainWindow);
    dock->setWindowTitle("Transient Close Test Dock");
    dock->show();
    QApplication::processEvents();

    dock->setTransient(true);
    QSignalSpy spyClosed(dock, &CutterDockWidget::closed);

    dock->close();

    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();

    QCOMPARE(spyClosed.count(), 1);
}

void TestDocking::cleanupTestCase()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();

    mainWindow = nullptr;
}

int main(int argc, char *argv[])
{
    char appName[] = "tst_Docking";
    char *cutterArgv[] = { appName, nullptr };
    int cutterArgc = 1;

    const CutterApplication app(cutterArgc, cutterArgv);
    qInstallMessageHandler(nullptr);

    TestDocking test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_Docking.moc"