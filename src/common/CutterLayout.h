#ifndef CUTTER_LAYOUT_H
#define CUTTER_LAYOUT_H

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QVariantMap>

/**
 * @brief Utilities related to cutter layout
 */
namespace Layout {

struct Config
{
    QByteArray geometry;
    QByteArray state;
    QMap<QString, QVariantMap> viewProperties;
};

const QString layoutDefault = "Default";
const QString layoutDebug = "Debug";

bool isBuiltinLayoutName(const QString &name);

}
#endif
