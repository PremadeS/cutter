#ifndef DEFAULTSHORTCUTS_H
#define DEFAULTSHORTCUTS_H

#include <QString>
#include <QKeySequence>
#include <QHash>
#include <QPair>

/**
 * @file DefaultShortcuts.h
 * Contains all of the default shortcuts for Cutter
 * Visually shown in @ref ShortcutOptionsWidget
 */

struct Shortcut
{
    QList<QKeySequence> keySequences;
    const char *text; // untranslated description
    const char *context;
};

const QHash<QString, Shortcut> &getDefaultShortcuts();

#endif // DEFAULTSHORTCUTS_H
