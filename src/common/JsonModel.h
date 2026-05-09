
#ifndef JSONMODEL_H
#define JSONMODEL_H

// TODO: REEEEEEEEEMOOOOOOOOOVEEEEEEEEEEEE THISSSSSSSSSSSSSSSS
// AND RUN CLANG_FROMAT ON ENTIRE PRJ WITH header sorting

#include <QTreeWidgetItem>
#include "CutterJson.h"

namespace Cutter {

QTreeWidgetItem *jsonTreeWidgetItem(const QString &key, const CutterJson &json);
};

#endif // JSONMODEL_H
