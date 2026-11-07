/**
 * @file test_GraphView.cpp
 * @brief The graph view scrolls, resizes and exports the way the user expects.
 *
 * - Dragging when zoomed in rounded every mouse step to whole graph units, so a slow drag
 *   didn't move the graph, and arrow keys stopped scrolling at very high zoom.
 * - Making the view smaller kept an offset that was only valid for the bigger view, which
 *   could leave the graph outside the view until the next scroll made it jump back.
 * - Exporting an image or SVG that couldn't be written failed without telling the user.
 * - Ctrl/Cmd+wheel only zoomed for whole wheel notches, so trackpads didn't zoom.
 * - Unconditional edges were drawn in the true branch color: rizin's missing jump/fail target is
 *   UT64_MAX, not 0, and the color Cutter asked for (`graph.trufae`) isn't in rizin's palette.
 */

#include "Fixtures.h"
#include "TestEnv.h"
#include "UiHelpers.h"
#include "ViewHelpers.h"

#include "common/Configuration.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTest>
#include <QWheelEvent>

namespace {

constexpr int viewRefreshTimeoutMs = 10000;
// Zoomed in far enough that a 1px mouse step is less than half a graph unit
constexpr qreal dragScale = 3.0;
constexpr int dragSteps = 30;
// Zoomed in far enough that an arrow key step (30 units / scale) is less than one unit
constexpr qreal farZoomScale = 40.0;
constexpr int keyPresses = 4;
// Shrunk below the 3/4 of the view the graph can be pushed to
constexpr int shrinkDivisor = 2;
constexpr int farAway = 100000;
// A trackpad sends a fraction of a wheel notch (120) per event
constexpr int trackpadDelta = 30;
constexpr int trackpadEvents = 4;
// CutterGraphView zooms by this factor per notch
constexpr qreal zoomPerNotch = 1.25;

void sendMouse(QWidget *widget, QEvent::Type type, const QPoint &pos, Qt::MouseButton button,
               Qt::MouseButtons buttons)
{
    QMouseEvent event(type, pos, widget->mapToGlobal(pos), button, buttons, Qt::NoModifier);
    QApplication::sendEvent(widget, &event);
}

void sendKey(QWidget *widget, Qt::Key key)
{
    QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
    QApplication::sendEvent(widget, &event);
}

void sendZoomWheel(QWidget *widget, int angle)
{
    const QPointF pos(widget->width() / 2.0, widget->height() / 2.0);
    QWheelEvent event(pos, widget->mapToGlobal(pos), QPoint(), QPoint(0, angle), Qt::NoButton,
                      Qt::ControlModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(widget, &event);
}

/** Union of all blocks, in view coordinates. */
QRect blocksInView(DisassemblerGraphView *view)
{
    QRect result;
    for (const auto &it : view->getBlocks()) {
        const auto &block = it.second;
        const QPoint topLeft = view->logicalToViewCoordinates(QPoint(block.x, block.y));
        const QPoint bottomRight = view->logicalToViewCoordinates(
                QPoint(block.x + block.width, block.y + block.height));
        result |= QRect(topLeft, bottomRight);
    }
    return result;
}

} // namespace

class TestGraphView : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void dragFollowsCursorWhenZoomedIn();
    void arrowKeysScrollWhenZoomedFarIn();
    void shrinkingTheViewKeepsTheGraphInView();
    void exportReportsFailure();
    void wheelZoomsByFractionsOfANotch();
    void unconditionalEdgesUseTheJumpColor();

private:
    GraphWidget *graph = nullptr;
    DisassemblerGraphView *view = nullptr;
    RVA mainStart = RVA_INVALID;
};

void TestGraphView::initTestCase()
{
    MainWindow *main = CutterTest::mainWindow();
    QVERIFY(main);
    graph = CutterTest::findSynchronizedDock<GraphWidget>(main);
    QVERIFY(graph);
    view = graph->getGraphView();
    QVERIFY(view);

    const QList<FunctionDescription> functions = CutterTest::core()->getAllFunctions();
    for (const FunctionDescription &function : functions) {
        if (function.name == QLatin1String("main") || function.name == QLatin1String("sym.main")) {
            mainStart = function.offset;
        }
    }
    QVERIFY2(mainStart != RVA_INVALID, "no main function");
}

void TestGraphView::init()
{
    graph->raiseMemoryWidget();
    CutterTest::core()->seek(mainStart);
    QTRY_VERIFY_WITH_TIMEOUT(CutterTest::graphHasBlock(graph, mainStart), viewRefreshTimeoutMs);
    view->setViewScale(1.0);
    view->center();
}

void TestGraphView::dragFollowsCursorWhenZoomedIn()
{
    view->setViewScale(dragScale);
    view->center();
    QWidget *viewport = view->viewport();
    const QPoint start = viewport->rect().center();
    const QPoint before = view->getViewOffset();

    // Dragging right and down moves the offset towards the lower bound, which centering
    // leaves the most room for
    QPoint pos = start;
    sendMouse(viewport, QEvent::MouseButtonPress, pos, Qt::MiddleButton, Qt::MiddleButton);
    for (int i = 0; i < dragSteps; ++i) {
        pos += QPoint(1, 1);
        sendMouse(viewport, QEvent::MouseMove, pos, Qt::NoButton, Qt::MiddleButton);
    }
    sendMouse(viewport, QEvent::MouseButtonRelease, pos, Qt::MiddleButton, Qt::NoButton);

    const QPoint moved = before - view->getViewOffset();
    const int expected = qRound(dragSteps / dragScale);
    QVERIFY2(qAbs(moved.x() - expected) <= 1 && qAbs(moved.y() - expected) <= 1,
             qPrintable(QStringLiteral("dragging %1px moved the graph by (%2, %3) units, "
                                       "expected %4")
                                .arg(dragSteps)
                                .arg(moved.x())
                                .arg(moved.y())
                                .arg(expected)));
}

void TestGraphView::arrowKeysScrollWhenZoomedFarIn()
{
    view->setViewScale(farZoomScale);
    view->center();
    const QPoint before = view->getViewOffset();

    for (int i = 0; i < keyPresses; ++i) {
        sendKey(view, Qt::Key_Down);
    }
    QVERIFY2(view->getViewOffset().y() > before.y(), "Down didn't scroll the zoomed in graph");
}

void TestGraphView::shrinkingTheViewKeepsTheGraphInView()
{
    // Push the graph as far right and down as scrolling allows
    view->setViewOffset(QPoint(-farAway, -farAway));
    QVERIFY(blocksInView(view).intersects(view->viewport()->rect()));

    const QSize original = view->size();
    const auto restore = qScopeGuard([&]() { view->resize(original); });
    view->resize(original / shrinkDivisor);
    QCOMPARE(view->size(), original / shrinkDivisor);

    QVERIFY2(blocksInView(view).intersects(view->viewport()->rect()),
             "the graph is outside the view after making it smaller");
}

void TestGraphView::exportReportsFailure()
{
    using ExportType = CutterGraphView::GraphExportType;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString missingDir = dir.filePath(QStringLiteral("missing"));

    const QList<QPair<ExportType, QString>> formats = {
        { ExportType::Png, QStringLiteral("graph.png") },
        { ExportType::Jpeg, QStringLiteral("graph.jpg") },
        { ExportType::Svg, QStringLiteral("graph.svg") },
    };
    for (const auto &format : formats) {
        const QString written = dir.filePath(format.second);
        {
            CutterTest::MessageBoxWatcher watcher;
            view->exportGraph(written, format.first, RZ_CORE_GRAPH_TYPE_BLOCK_FUN, mainStart);
            QVERIFY2(watcher.seenTexts().isEmpty(),
                     qPrintable(format.second + " export warned although it worked"));
        }
        QVERIFY2(QFileInfo(written).size() > 0, qPrintable(format.second + " wasn't written"));

        CutterTest::MessageBoxWatcher watcher;
        view->exportGraph(QDir(missingDir).filePath(format.second), format.first,
                          RZ_CORE_GRAPH_TYPE_BLOCK_FUN, mainStart);
        QVERIFY2(watcher.seenTexts().size() == 1,
                 qPrintable(format.second + " export into a missing folder didn't warn"));
    }
}

void TestGraphView::wheelZoomsByFractionsOfANotch()
{
    QWidget *viewport = view->viewport();

    sendZoomWheel(viewport, QWheelEvent::DefaultDeltasPerStep);
    QVERIFY2(qFuzzyCompare(view->getViewScale(), zoomPerNotch),
             "one wheel notch doesn't zoom one step");

    const qreal before = view->getViewScale();
    for (int i = 0; i < trackpadEvents; ++i) {
        sendZoomWheel(viewport, trackpadDelta);
    }
    QVERIFY2(view->getViewScale() > before, "small wheel steps didn't zoom in");

    const qreal zoomedIn = view->getViewScale();
    for (int i = 0; i < trackpadEvents; ++i) {
        sendZoomWheel(viewport, -trackpadDelta);
    }
    QVERIFY2(view->getViewScale() < zoomedIn, "small wheel steps didn't zoom out");
}

void TestGraphView::unconditionalEdgesUseTheJumpColor()
{
    const QColor jumpColor = ConfigColor("graph.ujump");
    const QColor trueColor = ConfigColor("graph.true");
    QVERIFY(jumpColor.isValid());
    QVERIFY2(jumpColor != trueColor, "the color theme draws both edges the same");

    // Find a block with a single successor: it ends in a jump or falls into the next block
    int checked = 0;
    const QList<FunctionDescription> functions = CutterTest::core()->getAllFunctions();
    for (const FunctionDescription &function : functions) {
        CutterTest::core()->seek(function.offset);
        QTRY_VERIFY_WITH_TIMEOUT(CutterTest::graphHasBlock(graph, function.offset),
                                 viewRefreshTimeoutMs);
        const auto edges = view->getEdgeConfigurations();
        for (const auto &it : view->getBlocks()) {
            const auto &block = it.second;
            if (block.edges.size() != 1) {
                continue;
            }
            const auto edge = edges.find({ block.entry, block.edges.front().target });
            QVERIFY(edge != edges.end());
            QVERIFY2(edge->second.color == jumpColor,
                     qPrintable(QStringLiteral("the only edge of %1 isn't drawn as a jump")
                                        .arg(rzAddressString(block.entry))));
            ++checked;
        }
        if (checked > 0) {
            break;
        }
    }
    QVERIFY2(checked > 0, "no function has a block with a single successor");
}

CUTTER_APP_TEST_MAIN_WITH_FILE(TestGraphView, CutterTest::Fixtures::helloX64())
#include "test_GraphView.moc"
