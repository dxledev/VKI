#pragma once
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace vki {

struct KeyDef {
    int row = 0, col = 0;
    double x = 0, y = 0, w = 1, h = 1;
    int encoder = -1;  // rotary encoder index, or -1 for an ordinary key
};

struct Control {
    enum Type { Range, Dropdown, Color, Toggle } type = Range;
    QString label, valueName, showIf;
    int channel = 0, id = 0;
    int min = 0, max = 255;
    QList<QPair<QString, int>> options;
};

struct Section {
    QString label;
    QList<Control> controls;
};

struct CustomKeycode {
    QString name, title, shortName;
};

struct Definition {
    QString name;
    quint16 vendorId = 0, productId = 0;
    int rows = 0, cols = 0;
    QList<KeyDef> keys;
    QList<Section> sections;
    QList<CustomKeycode> customKeycodes;
    double width() const;
    double height() const;
};

// Loads a VIA definition from a file path or a Qt resource path.
Definition loadDefinition(const QString &path);
// Evaluates a showIf expression such as "{effect} != 0 && {effect} != 24".
bool evaluateShowIf(const QString &expression, const QMap<QString, int> &values);

}
