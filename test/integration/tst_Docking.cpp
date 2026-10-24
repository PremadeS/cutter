#include "CutterApplication.h"
#include "CutterDockWidget.h"
#include "core/MainWindow.h"
#include "shortcuts/ShortcutManager.h"

#include <QCoreApplication>
#include <QMouseEvent>
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
    QTest::qWait(500);
}

void TestDocking::testDockingPreventedByModifier()
{
    auto *dock = new CutterDockWidget(mainWindow);
    dock->setWindowTitle("Automated Test Dock");
    mainWindow->addDockWidget(Qt::LeftDockWidgetArea, dock);

    // 1. Initial State: Floating
    dock->setFloating(true);
    dock->show();
    dock->raise();
    QApplication::processEvents();
    QTest::qWait(1000); // Visual pause: Dock is floating

    QCOMPARE(dock->isFloating(), true);

    QWindow *win = mainWindow->windowHandle();
    QVERIFY2(win != nullptr, "MainWindow native QWindow handle is null");

    const Qt::KeyboardModifier mod =
            Shortcuts()->convertKeyToModifer(Shortcuts()->getKeySequence("Docking.toggle"));

    const Qt::Key keyToPress = (mod == Qt::ControlModifier) ? Qt::Key_Control
            : (mod == Qt::ShiftModifier)                    ? Qt::Key_Shift
            : (mod == Qt::AltModifier)                      ? Qt::Key_Alt
                                                            : Qt::Key_Meta;

    const QPoint dragPoint(50, 50);

    // ------------------------------------------------------------------
    // 2. DRAG & DROP WITH MODIFIER HELD (Docking PREVENTED)
    // ------------------------------------------------------------------
    QTest::keyPress(win, keyToPress, mod);

    QMoveEvent moveEvent(QPoint(100, 100), QPoint(0, 0));
    QCoreApplication::sendEvent(dock, &moveEvent);
    QTest::qWait(1000); // Visual pause: Modifier key active during drag

    QMouseEvent dropWithMod(QEvent::MouseButtonRelease, dragPoint, Qt::LeftButton, Qt::LeftButton,
                            mod);
    QCoreApplication::sendEvent(dock, &dropWithMod);
    QApplication::processEvents();
    QTest::qWait(1000); // Visual pause: Drop attempted (should stay floating)

    QCOMPARE(dock->isFloating(), true);

    // ------------------------------------------------------------------
    // 3. DRAG & DROP WITHOUT MODIFIER (Docking SUCCEEDS)
    // ------------------------------------------------------------------
    QTest::keyRelease(win, keyToPress, Qt::NoModifier);

    QCoreApplication::sendEvent(dock, &moveEvent);
    QTest::qWait(1000); // Visual pause: Modifier key released

    mainWindow->addDockWidget(Qt::LeftDockWidgetArea, dock);
    dock->setFloating(false);
    QApplication::processEvents();
    QTest::qWait(1000); // Visual pause: Widget docked into main layout

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
    QTest::qWait(500);

    const int initialCount = spyVisible.count();

    dock->ignoreVisibilityStatus(true);
    dock->hide();
    QTest::qWait(300);
    dock->show();
    QApplication::processEvents();
    QTest::qWait(500);

    QCOMPARE(spyVisible.count(), initialCount);

    dock->deleteLater();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

void TestDocking::testTransientDockWidgetClose()
{
    auto *dock = new CutterDockWidget(mainWindow);
    dock->setWindowTitle("Transient Close Test Dock");
    dock->show();
    QTest::qWait(500);

    dock->setTransient(true);
    QSignalSpy spyClosed(dock, &CutterDockWidget::closed);

    dock->close();

    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();
    QTest::qWait(500); // Visual pause: Transient widget deleted

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