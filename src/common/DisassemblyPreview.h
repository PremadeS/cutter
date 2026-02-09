#ifndef DISASSEMBLYPREVIEW_H
#define DISASSEMBLYPREVIEW_H

#include <QTextBlockUserData>

#include "core/CutterDescriptions.h"

class QWidget;

class DisassemblyTextBlockUserData : public QTextBlockUserData
{
public:
    DisassemblyLine line;

    explicit DisassemblyTextBlockUserData(const DisassemblyLine &line);
};

DisassemblyTextBlockUserData *getUserData(const QTextBlock &block);

/**
 * @brief Namespace to define relevant functions
 *
 * @ingroup DisassemblyPreview
 */
namespace DisassemblyPreview {
/*!
 * @brief Get the QString that defines the stylesheet for tooltip
 * @return A QString for the stylesheet
 */
QString getToolTipStyleSheet();

/*!
 * @brief Show a QToolTip that previews the disassembly that is pointed to
 * It works for GraphWidget and DisassemblyWidget
 * @return True if the tooltip is shown
 */
bool showDisasPreview(QWidget *parent, const QPoint &pointOfEvent, const RVA offsetFromk);

/**
 * @brief Show a QToolTip that previews the disassembly at a specific address
 * @return True if the tooltip is shown
 */
bool showDisasPreviewAt(QWidget *parent, const QPoint &pointOfEvent, const RVA offset);

/**
 * @brief Finds the source (from) address of an XRef based on the text word under the cursor
 * @param offset The base offset of the line which contains an XREF to it
 * @param selectedWord The specific text string being hovered (must be an address)
 * @return The source RVA of the XRef, or RVA_INVALID if not found
 */
RVA getXRefFromWord(RVA offset, const QString &selectedWord);

/**
 * @brief Checks if a disassembly line is an auto-generated XRef metadata line
 * @param offset The offset of the current disassembly line/block
 * @param line The full text content of the disassembly line/block
 * @return True if the line is an analysis-generated XRef comment
 */
bool isXRefFromComment(RVA offset, const QString &line);

/*!
 * @brief Reads the offset for the cursor position
 * @return The disassembly offset of the hovered asm text
 */
RVA readDisassemblyOffset(QTextCursor tc);

/**
 * @brief Show a QToolTip that shows the value of the highlighted register, variable, or memory
 * @return True if the tooltip is shown
 */
bool showDebugValueTooltip(QWidget *parent, const QPoint &pointOfEvent, const QString &selectedText,
                           const RVA offset);

}
#endif
