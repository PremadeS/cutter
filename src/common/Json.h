#ifndef JSON_H
#define JSON_H

#include "CutterCommon.h"

#include <QJsonValue>

class QTreeWidgetItem;
class CutterJson;

/**
 * @brief Helpers for Json related objects
 *
 * TODO: maybe cutter helpers??? as a namespace name
 */
namespace Json {

inline RVA jsonValueToRVA(const QJsonValue &value, RVA defaultValue = RVA_INVALID)
{
    bool ok;
    const RVA ret = value.toVariant().toULongLong(&ok);
    if (!ok) {
        return defaultValue;
    }
    return ret;
}

QTreeWidgetItem *jsonTreeWidgetItem(const QString &key, const CutterJson &json);

}

#endif // JSON_H
