#include "definition.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QRegularExpression>
#include <algorithm>
#include <stdexcept>

namespace vki {
namespace {

[[noreturn]] void fail(const QString &message) {
    throw std::invalid_argument(message.toStdString());
}

QString prettify(QString name) {
    name.replace('_', ' ');
    bool upper = true;
    for (auto &c : name) {
        if (upper && c.isLetter())
            c = c.toUpper();
        upper = c == ' ';
    }
    return name;
}

void parseLayout(const QJsonArray &rows, Definition &result) {
    double y = 0;
    for (const auto &rowValue : rows) {
        if (!rowValue.isArray())
            continue;
        double x = 0, w = 1, h = 1;
        for (const auto &item : rowValue.toArray()) {
            if (item.isObject()) {
                const auto props = item.toObject();
                x += props.value("x").toDouble();
                y += props.value("y").toDouble();
                if (props.contains("w")) w = props.value("w").toDouble(1);
                if (props.contains("h")) h = props.value("h").toDouble(1);
            } else if (item.isString()) {
                const auto label = item.toString().split('\n').first();
                const auto parts = label.split(',');
                bool rowOk = false, colOk = false;
                const int row = parts.value(0).trimmed().toInt(&rowOk);
                const int col = parts.value(1).trimmed().toInt(&colOk);
                if (parts.size() == 2 && rowOk && colOk) {
                    KeyDef key;
                    key.row = row; key.col = col;
                    key.x = x; key.y = y; key.w = w; key.h = h;
                    // VIA marks a knob with an "e<index>" legend in the ninth label slot.
                    const auto labels = item.toString().split('\n');
                    const auto mark = labels.value(9).trimmed();
                    bool indexOk = false;
                    if (mark.size() > 1 && (mark[0] == 'e' || mark[0] == 'E')) {
                        const int index = mark.mid(1).toInt(&indexOk);
                        if (indexOk && index >= 0 && index < 64) key.encoder = index;
                    }
                    result.keys.append(key);
                }
                x += w;
                w = 1; h = 1;
            }
        }
        y += 1;
    }
}

Control parseControl(const QJsonObject &object) {
    Control control;
    control.label = object.value("label").toString();
    control.showIf = object.value("showIf").toString();
    const auto type = object.value("type").toString();
    const auto content = object.value("content").toArray();
    if (content.size() < 3)
        fail("Control without a value address: " + control.label);
    control.valueName = content[0].toString();
    control.channel = content[1].toInt();
    control.id = content[2].toInt();
    const auto options = object.value("options").toArray();
    if (type == "range") {
        control.type = Control::Range;
        control.min = options.at(0).toInt(0);
        control.max = options.at(1).toInt(255);
    } else if (type == "dropdown") {
        control.type = Control::Dropdown;
        for (const auto &option : options) {
            const auto pair = option.toArray();
            control.options.append({prettify(pair.at(0).toString()), pair.at(1).toInt()});
        }
    } else if (type == "color") {
        control.type = Control::Color;
    } else if (type == "toggle") {
        control.type = Control::Toggle;
    } else {
        fail("Unsupported control type: " + type);
    }
    return control;
}

void collectControls(const QJsonArray &content, Section &section, QList<Section> &out) {
    for (const auto &item : content) {
        const auto object = item.toObject();
        const auto inner = object.value("content");
        if (object.contains("type")) {
            section.controls.append(parseControl(object));
        } else if (inner.isArray()) {
            Section child;
            child.label = object.value("label").toString();
            collectControls(inner.toArray(), child, out);
            if (!child.controls.isEmpty())
                out.append(child);
        }
    }
}

}

double Definition::width() const {
    double result = 0;
    for (const auto &key : keys) result = std::max(result, key.x + key.w);
    return result;
}

double Definition::height() const {
    double result = 0;
    for (const auto &key : keys) result = std::max(result, key.y + key.h);
    return result;
}

Definition loadDefinition(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        fail(path + ": " + file.errorString());
    if (file.size() > 1024 * 1024)
        fail(path + ": definition exceeds 1 MiB");
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        fail(path + ": invalid JSON (" + error.errorString() + ")");
    const auto root = document.object();
    Definition result;
    result.name = root.value("name").toString();
    bool ok = false;
    result.vendorId = root.value("vendorId").toString().toUShort(&ok, 16);
    if (!ok) fail("Definition has an invalid vendorId");
    result.productId = root.value("productId").toString().toUShort(&ok, 16);
    if (!ok) fail("Definition has an invalid productId");
    const auto matrix = root.value("matrix").toObject();
    result.rows = matrix.value("rows").toInt();
    result.cols = matrix.value("cols").toInt();
    if (result.rows < 1 || result.cols < 1 || result.rows > 32 || result.cols > 32)
        fail("Definition has an invalid matrix size");
    parseLayout(root.value("layouts").toObject().value("keymap").toArray(), result);
    if (result.keys.isEmpty())
        fail("Definition has no keys");
    for (const auto &key : std::as_const(result.keys))
        if (key.row < 0 || key.row >= result.rows || key.col < 0 || key.col >= result.cols || key.w <= 0 || key.h <= 0)
            fail("Definition has a key outside the matrix or with an invalid size");
    for (const auto &menu : root.value("menus").toArray()) {
        Section top;
        top.label = menu.toObject().value("label").toString();
        collectControls(menu.toObject().value("content").toArray(), top, result.sections);
        if (!top.controls.isEmpty())
            result.sections.append(top);
    }
    for (const auto &custom : root.value("customKeycodes").toArray()) {
        const auto object = custom.toObject();
        result.customKeycodes.append({object.value("name").toString(), object.value("title").toString(),
                                      object.value("shortName").toString()});
    }
    return result;
}

bool evaluateShowIf(const QString &expression, const QMap<QString, int> &values) {
    if (expression.trimmed().isEmpty())
        return true;
    static const QRegularExpression term(R"(\{(\w+)\}\s*(==|!=)\s*(-?\d+))");
    bool result = true;
    for (const auto &clause : expression.split("&&")) {
        const auto match = term.match(clause);
        if (!match.hasMatch())
            continue;
        const int value = values.value(match.captured(1), 0);
        const int expected = match.captured(3).toInt();
        const bool holds = match.captured(2) == "==" ? value == expected : value != expected;
        result = result && holds;
    }
    return result;
}

}
