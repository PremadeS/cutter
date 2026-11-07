/**
 * @file test_Disassembly.cpp
 * @brief The Disassembly view's context menu, copy and left panel act on what the user sees.
 *
 * - Delete flag deleted the flag of the line instead of the flag the menu names (an operand).
 * - Edit function wrote the new start address into the function without telling rizin, so
 *   rizin kept finding the function at its old start (rename, the disassembly header).
 * - Copying a selection that starts before the lines Cutter keeps copied HTML (`&lt;`).
 * - Scrolling over the left panel (jump arrows) went the wrong way for small (trackpad) steps and
 *   scrolled down on a sideways swipe.
 */

#include "Fixtures.h"
#include "TestEnv.h"
#include "UiHelpers.h"
#include "ViewHelpers.h"

#include "dialogs/EditFunctionDialog.h"
#include "menus/DisassemblyContextMenu.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFrame>
#include <QPlainTextEdit>
#include <QScopeGuard>
#include <QTest>
#include <QTextBlock>
#include <QWheelEvent>

namespace {

constexpr int viewRefreshTimeoutMs = 10000;
constexpr int linesBeforeComment = 5;
constexpr int linesToScrollFirst = 10;
constexpr int wheelSteps = 4;
// Smaller than one line's worth (40) and than the old code's 75, as a trackpad sends
constexpr int smallWheelDelta = 30;
const QString lineFlag = QStringLiteral("cutter_test_line_flag");
const QString commentWithMarkup = QStringLiteral("cutter<test>");

/** The menu's action with this text, or whose text starts with it when \p prefix is set. */
QAction *menuAction(DisassemblyContextMenu *menu, const QString &text, bool prefix = false)
{
    const QList<QAction *> actions = menu->findChildren<QAction *>();
    for (QAction *action : actions) {
        if (prefix ? action->text().startsWith(text) : action->text() == text) {
            return action;
        }
    }
    return nullptr;
}

QList<FlagDescription> flagsAt(RVA address)
{
    QList<FlagDescription> result;
    const QList<FlagDescription> flags = CutterTest::core()->getAllFlags();
    for (const FlagDescription &flag : flags) {
        if (flag.offset == address) {
            result << flag;
        }
    }
    return result;
}

void sendWheel(QWidget *widget, const QPoint &angleDelta)
{
    const QPointF pos(widget->width() / 2.0, widget->height() / 2.0);
    QWheelEvent event(pos, widget->mapToGlobal(pos), QPoint(), angleDelta, Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(widget, &event);
}

} // namespace

class TestDisassembly : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void deleteFlagDeletesTheFlagTheMenuNames();
    void editFunctionMovesTheFunctionStart();
    void editFunctionRefusesAnotherFunctionsStart();
    void copyBeyondKeptLinesIsPlainText();
    void leftPanelWheelScrollsLikeTheText();

private:
    void editFunctionStart(const QString &start);

    MainWindow *main = nullptr;
    DisassemblyWidget *disassembly = nullptr;
    DisassemblyContextMenu *menu = nullptr;
    RVA mainStart = RVA_INVALID;
    RVA otherFunction = RVA_INVALID;
};

void TestDisassembly::initTestCase()
{
    main = CutterTest::mainWindow();
    QVERIFY(main);
    disassembly = CutterTest::findSynchronizedDock<DisassemblyWidget>(main);
    QVERIFY(disassembly);
    menu = disassembly->findChild<DisassemblyContextMenu *>();
    QVERIFY(menu);

    const QList<FunctionDescription> functions = CutterTest::core()->getAllFunctions();
    for (const FunctionDescription &function : functions) {
        if (function.name == QLatin1String("main") || function.name == QLatin1String("sym.main")) {
            mainStart = function.offset;
        }
    }
    for (const FunctionDescription &function : functions) {
        if (function.offset != mainStart) {
            otherFunction = function.offset;
            break;
        }
    }
    QVERIFY2(mainStart != RVA_INVALID, "no main function");
    QVERIFY(otherFunction != RVA_INVALID);
}

void TestDisassembly::init()
{
    disassembly->raiseMemoryWidget();
    CutterTest::core()->seek(mainStart);
    // A test may have scrolled the view without seeking; the seek above is then a no-op
    disassembly->refreshDisasm(mainStart);
    QTRY_VERIFY_WITH_TIMEOUT(CutterTest::disassemblyOffsets(disassembly).contains(mainStart),
                             viewRefreshTimeoutMs);
}

void TestDisassembly::deleteFlagDeletesTheFlagTheMenuNames()
{
    CutterCore *core = CutterTest::core();

    // An instruction whose operand points at a string flag, e.g. `lea rdi, str.hello`
    RVA line = RVA_INVALID;
    RVA target = RVA_INVALID;
    const QList<FlagDescription> strings = core->getAllFlags(RZ_FLAGS_FS_STRINGS);
    for (const FlagDescription &string : strings) {
        const QList<XrefDescription> xrefs = core->getXRefs(string.offset, true, false);
        for (const XrefDescription &xref : xrefs) {
            if (core->functionIn(xref.from) && !core->functionAt(string.offset)) {
                line = xref.from;
                target = string.offset;
                break;
            }
        }
        if (line != RVA_INVALID) {
            break;
        }
    }
    QVERIFY2(line != RVA_INVALID, "no instruction referencing a string flag");

    // A flag on the line itself, which the old code deleted instead
    const QList<FlagDescription> targetFlags = flagsAt(target);
    core->addFlag(line, lineFlag, 1);
    const auto restore = qScopeGuard([&]() {
        core->delFlag(lineFlag);
        QStringList present;
        for (const FlagDescription &flag : flagsAt(target)) {
            present << flag.name;
        }
        for (const FlagDescription &flag : targetFlags) {
            if (!present.contains(flag.name)) {
                core->addFlag(flag.offset, flag.name, flag.size);
            }
        }
    });

    core->seek(line);
    menu->setCurHighlightedWord(rzAddressString(target));
    menu->popup(disassembly->mapToGlobal(QPoint()));
    QTRY_VERIFY(menu->isVisible());
    QAction *deleteFlag = menuAction(menu, tr("Delete flag"));
    QVERIFY(deleteFlag);
    QVERIFY2(deleteFlag->isVisible(), "the menu doesn't offer to delete the operand's flag");
    menu->hide();

    deleteFlag->trigger();

    QCOMPARE(flagsAt(target).size(), targetFlags.size() - 1);
    bool lineFlagKept = false;
    for (const FlagDescription &flag : flagsAt(line)) {
        lineFlagKept = lineFlagKept || flag.name == lineFlag;
    }
    QVERIFY2(lineFlagKept, "the flag on the line was deleted instead");
}

void TestDisassembly::editFunctionStart(const QString &start)
{
    CutterTest::core()->seek(mainStart);
    QAction *editFunction = menuAction(menu, tr("Edit function"), true);
    QVERIFY(editFunction);

    CutterTest::onceVisible<EditFunctionDialog>(this, [start](EditFunctionDialog *dialog) {
        CutterTest::setLineEditText(dialog, "startLineEdit", start);
        CutterTest::clickStandardButton(dialog, QDialogButtonBox::Ok);
    });
    editFunction->trigger();
}

void TestDisassembly::editFunctionMovesTheFunctionStart()
{
    CutterCore *core = CutterTest::core();
    const QString name = core->functionAt(mainStart)->name;
    const RVA newStart = core->nextOpAddr(mainStart, 1);
    QVERIFY(newStart != mainStart);
    const auto restore = qScopeGuard([&]() {
        RzCoreLocked locked(core);
        RzAnalysisFunction *fcn = rz_analysis_get_function_at(locked->analysis, newStart);
        if (!fcn) {
            return;
        }
        rz_analysis_function_relocate(fcn, mainStart);
        RzFlagItem *flag = rz_flag_get(locked->flags, fcn->name);
        if (flag && flag->offset == newStart) {
            rz_flag_set(locked->flags, flag->name, mainStart, flag->size);
        }
    });

    editFunctionStart(rzAddressString(newStart));

    const RzAnalysisFunction *moved = core->functionAt(newStart);
    QVERIFY2(moved, "rizin doesn't find the function at its new start");
    QCOMPARE(QString(moved->name), name);
    QVERIFY2(!core->functionAt(mainStart), "rizin still finds the function at its old start");
}

void TestDisassembly::editFunctionRefusesAnotherFunctionsStart()
{
    CutterCore *core = CutterTest::core();
    const QString name = core->functionAt(mainStart)->name;
    const QString otherName = core->functionAt(otherFunction)->name;
    CutterTest::MessageBoxWatcher messages;

    editFunctionStart(rzAddressString(otherFunction));

    QTRY_VERIFY(!messages.seenTexts().isEmpty());
    QVERIFY(core->functionAt(mainStart));
    QCOMPARE(QString(core->functionAt(mainStart)->name), name);
    QCOMPARE(QString(core->functionAt(otherFunction)->name), otherName);
}

void TestDisassembly::copyBeyondKeptLinesIsPlainText()
{
    CutterCore *core = CutterTest::core();
    const RVA commented = core->nextOpAddr(mainStart, linesBeforeComment);
    QVERIFY(core->getCommentAt(commented).isEmpty());
    core->setComment(commented, commentWithMarkup);
    const auto restore = qScopeGuard([&]() { core->delComment(commented); });

    auto *edit = qobject_cast<QPlainTextEdit *>(disassembly->getTextWidget());
    QVERIFY(edit);

    // Start the selection at main, then show only the lines from the comment on, so the selection
    // starts before the lines the view keeps
    disassembly->refreshDisasm(mainStart);
    QCOMPARE(CutterTest::disassemblyTopOffset(disassembly), mainStart);
    disassembly->updateSelectionAnchor(QTextCursor(edit->document()));
    disassembly->refreshDisasm(commented);
    QVERIFY(disassembly->getLines().first().offset > mainStart);
    const int visibleLines = disassembly->getEndIndex() - disassembly->getStartIndex();
    QTextCursor end(edit->document()->findBlockByNumber(visibleLines - 1));
    end.movePosition(QTextCursor::EndOfBlock);
    disassembly->updateSelectionPos(end);
    disassembly->refreshDisasm(RVA_INVALID, RefreshMode::None);

    QApplication::clipboard()->clear();
    QAction *copy = menuAction(menu, tr("Copy"));
    QVERIFY(copy);
    copy->trigger();

    const QString copied = QApplication::clipboard()->text();
    QVERIFY2(copied.contains(commentWithMarkup), qPrintable(copied));
    QVERIFY2(!copied.contains("&lt;") && !copied.contains("&nbsp;"), qPrintable(copied));
}

void TestDisassembly::leftPanelWheelScrollsLikeTheText()
{
    QWidget *panel = nullptr;
    const QList<QFrame *> frames = disassembly->findChildren<QFrame *>();
    for (QFrame *frame : frames) {
        if (dynamic_cast<DisassemblyLeftPanel *>(frame)) {
            panel = frame;
        }
    }
    QVERIFY(panel);

    // Leave room above, so scrolling up can move
    for (int i = 0; i < linesToScrollFirst; ++i) {
        disassembly->scrollInstructions(1);
    }
    const RVA start = CutterTest::disassemblyTopOffset(disassembly);

    for (int i = 0; i < wheelSteps; ++i) {
        sendWheel(panel, QPoint(0, smallWheelDelta));
    }
    const RVA afterUp = CutterTest::disassemblyTopOffset(disassembly);
    QVERIFY2(afterUp < start, "scrolling up over the left panel didn't move the view up");

    for (int i = 0; i < wheelSteps; ++i) {
        sendWheel(panel, QPoint(smallWheelDelta, 0));
    }
    QCOMPARE(CutterTest::disassemblyTopOffset(disassembly), afterUp);

    for (int i = 0; i < wheelSteps; ++i) {
        sendWheel(panel, QPoint(0, -smallWheelDelta));
    }
    QVERIFY2(CutterTest::disassemblyTopOffset(disassembly) > afterUp,
             "scrolling down over the left panel didn't move the view down");
}

CUTTER_APP_TEST_MAIN_WITH_FILE(TestDisassembly, CutterTest::Fixtures::helloX64())
#include "test_Disassembly.moc"
