#pragma once
#include "definition.h"

#include <QList>
#include <QString>

namespace vki {

struct KeycodeEntry {
    quint16 code;
    QString label;
};

struct KeycodeCategory {
    QString name;
    QList<KeycodeEntry> entries;
};

QString keycodeLabel(quint16 code, const QList<CustomKeycode> &custom);
// Legend as printed on a keycap, the way VIA draws it: shifted symbol above the base one, short names for the rest.
QString keycapLabel(quint16 code, const QList<CustomKeycode> &custom);
QList<KeycodeCategory> keycodeCategories(int layers, const QList<CustomKeycode> &custom);

}
