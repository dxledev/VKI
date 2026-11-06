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
QList<KeycodeCategory> keycodeCategories(int layers, const QList<CustomKeycode> &custom);

}
