#include "keycodes.h"
#include <QHash>
#include <algorithm>

namespace vki {
namespace {

const char *const basicNames[] = {
    "A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z",
    "1","2","3","4","5","6","7","8","9","0","Enter","Esc","Bksp","Tab","Space","-","=","[","]","\\","#",";","'","`",",",".","/","Caps",
    "F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12","PrtSc","ScrLk","Pause","Ins","Home","PgUp","Del","End","PgDn",
    "Right","Left","Down","Up","NumLk","P/","P*","P-","P+","PEnt","P1","P2","P3","P4","P5","P6","P7","P8","P9","P0","P.","\\|","Menu"
};

struct Named { quint16 code; const char *label; };
const Named extras[] = {
    {0xA8,"Mute"},{0xA9,"Vol+"},{0xAA,"Vol-"},{0xAB,"Next"},{0xAC,"Prev"},{0xAD,"Stop"},{0xAE,"Play"},{0xAF,"Select"},
    {0xB0,"Eject"},{0xB1,"Mail"},{0xB2,"Calc"},{0xB3,"MyPC"},{0xBD,"Bri+"},{0xBE,"Bri-"},
    {0xE0,"LCtrl"},{0xE1,"LShift"},{0xE2,"LAlt"},{0xE3,"LGui"},{0xE4,"RCtrl"},{0xE5,"RShift"},{0xE6,"RAlt"},{0xE7,"RGui"}
};

constexpr quint16 customBase = 0x7E00;

}

QString keycodeLabel(quint16 code, const QList<CustomKeycode> &custom) {
    if (code == 0) return {};
    if (code == 0x7C16) return QStringLiteral("Esc`");
    if (code == 1) return QStringLiteral("\u25BD");
    if (code >= 0x04 && code <= 0x04 + 0x65 && code - 4 < int(sizeof basicNames / sizeof *basicNames))
        return basicNames[code - 4];
    if (code >= 0x68 && code <= 0x73) return QString("F%1").arg(code - 0x68 + 13);
    for (const auto &entry : extras)
        if (entry.code == code) return entry.label;
    const struct { quint16 base; const char *prefix; } layerOps[] = {
        {0x5200,"TO"},{0x5220,"MO"},{0x5240,"DF"},{0x5260,"TG"},{0x5280,"OSL"},{0x52C0,"TT"}};
    for (const auto &op : layerOps)
        if (code >= op.base && code < op.base + 0x20)
            return QString("%1(%2)").arg(op.prefix).arg(code - op.base);
    if (code >= customBase && code - customBase < custom.size()) {
        const auto &entry = custom[code - customBase];
        return entry.name.isEmpty() ? entry.shortName : entry.name;
    }
    return "0x" + QString("%1").arg(code, 4, 16, QChar('0')).toUpper();
}

QString keycapLabel(quint16 code, const QList<CustomKeycode> &custom) {
    static const QHash<QString, QString> names = {
        {"1", "!\n1"}, {"2", "@\n2"}, {"3", "#\n3"}, {"4", "$\n4"}, {"5", "%\n5"}, {"6", "^\n6"}, {"7", "&\n7"},
        {"8", "*\n8"}, {"9", "(\n9"}, {"0", ")\n0"}, {"-", "_\n-"}, {"=", "+\n="}, {"[", "{\n["}, {"]", "}\n]"},
        {"\\", "|\n\\"}, {";", ":\n;"}, {"'", "\"\n'"}, {"`", "~\n`"}, {",", "<\n,"}, {".", ">\n."}, {"/", "?\n/"},
        {"\\|", "|\n\\"}, {"Bksp", "Backspace"}, {"Caps", "Caps Lock"}, {"PrtSc", "Print"}, {"ScrLk", "Scroll Lock"},
        {"NumLk", "N.Lck"}, {"PEnt", "N.Ent"}, {"P/", "\u00F7"}, {"P*", "\u00D7"}, {"P-", "-"}, {"P+", "+"}, {"P.", "."},
        {"Right", "\u2192"}, {"Left", "\u2190"}, {"Down", "\u2193"}, {"Up", "\u2191"},
        {"LCtrl", "LCtl"}, {"RCtrl", "RCtl"}, {"LGui", "LWin"}, {"RGui", "RWin"}, {"Esc`", "Esc `"},
    };
    const QString base = keycodeLabel(code, custom);
    if (code >= 0x59 && code <= 0x62) return QString(QChar('0' + (code == 0x62 ? 0 : code - 0x58)));  // keypad digits
    return names.value(base, base);
}

QList<KeycodeCategory> keycodeCategories(int layers, const QList<CustomKeycode> &custom) {
    QList<KeycodeCategory> result;
    KeycodeCategory special{"Special", {{0x0000, "None"}, {0x0001, "Pass"}}};
    result.append(special);
    KeycodeCategory letters{"Letters", {}};
    for (quint16 c = 0x04; c <= 0x1D; ++c) letters.entries.append({c, keycodeLabel(c, custom)});
    KeycodeCategory numbers{"Numbers", {}};
    for (quint16 c = 0x1E; c <= 0x27; ++c) numbers.entries.append({c, keycodeLabel(c, custom)});
    KeycodeCategory punctuation{"Symbols", {}};
    for (quint16 c : {0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,0x30,0x31,0x33,0x34,0x35,0x36,0x37,0x38,0x39})
        punctuation.entries.append({quint16(c), keycodeLabel(quint16(c), custom)});
    KeycodeCategory function{"Function", {}};
    for (quint16 c = 0x3A; c <= 0x45; ++c) function.entries.append({c, keycodeLabel(c, custom)});
    for (quint16 c = 0x68; c <= 0x73; ++c) function.entries.append({c, keycodeLabel(c, custom)});
    KeycodeCategory navigation{"Navigation", {}};
    for (quint16 c = 0x46; c <= 0x52; ++c) navigation.entries.append({c, keycodeLabel(c, custom)});
    navigation.entries.append({0x65, "Menu"});
    KeycodeCategory numpad{"Numpad", {}};
    for (quint16 c = 0x53; c <= 0x63; ++c) numpad.entries.append({c, keycodeLabel(c, custom)});
    KeycodeCategory modifiers{"Modifiers", {}};
    for (quint16 c = 0xE0; c <= 0xE7; ++c) modifiers.entries.append({c, keycodeLabel(c, custom)});
    KeycodeCategory media{"Media", {}};
    for (const auto &entry : extras)
        if (entry.code >= 0xA8 && entry.code <= 0xBE) media.entries.append({entry.code, entry.label});
    KeycodeCategory layerCat{"Layers", {}};
    for (int layer = 0; layer < std::min(layers, 8); ++layer) {
        layerCat.entries.append({quint16(0x5220 + layer), QString("MO(%1)").arg(layer)});
        layerCat.entries.append({quint16(0x5260 + layer), QString("TG(%1)").arg(layer)});
        layerCat.entries.append({quint16(0x5200 + layer), QString("TO(%1)").arg(layer)});
    }
    result << letters << numbers << punctuation << function << navigation << numpad << modifiers << media;
    if (layers > 1) result << layerCat;
    if (!custom.isEmpty()) {
        KeycodeCategory device{"Keyboard", {}};
        for (int i = 0; i < custom.size(); ++i)
            device.entries.append({quint16(customBase + i), keycodeLabel(quint16(customBase + i), custom)});
        result.append(device);
    }
    return result;
}

}
