#include "core/theme.h"
#include "core/theme_source.h"
#include "device/definition.h"
#include "device/keycodes.h"
#include <QtTest>

class CoreTests : public QObject {
    Q_OBJECT
private slots:
    void alphaFirstColor() {
        auto theme = vki::setColors(vki::presetTheme("lofi"), {"background=#e00e1e36"});
        const auto c = theme.color("background");
        QCOMPARE(c.red(), 0x0e); QCOMPARE(c.green(), 0x1e); QCOMPARE(c.blue(), 0x36); QCOMPARE(c.alpha(), 0xe0);
    }
    void qtRgba() {
        auto theme = vki::setColors(vki::presetTheme("lofi"), {"background=Qt.rgba(14 / 255, 30 / 255, 54 / 255, 0.88)"});
        const auto c = theme.color("background");
        QCOMPARE(c.red(), 14); QCOMPARE(c.green(), 30); QCOMPARE(c.blue(), 54); QCOMPARE(c.alpha(), 224);
    }
    void roundTripKeepsAlpha() {
        auto theme = vki::setColors(vki::presetTheme("lofi"), {"background=#e00e1e36"});
        const auto again = vki::parseTheme(theme.toJson());
        QCOMPARE(again.color("background"), theme.color("background"));
    }
    void definitionLoads() {
        const auto d = vki::loadDefinition(":/vki/galaxy100-lite.json");
        QCOMPARE(d.vendorId, quint16(0x36B0)); QCOMPARE(d.productId, quint16(0x3055));
        QVERIFY(d.keys.size() > 90);
        QVERIFY(!d.sections.isEmpty());
        QMap<QString, int> v{{"id_qmk_rgb_matrix_effect", 0}};
        QVERIFY(!vki::evaluateShowIf("{id_qmk_rgb_matrix_effect} != 0", v));
        v["id_qmk_rgb_matrix_effect"] = 5;
        QVERIFY(vki::evaluateShowIf("{id_qmk_rgb_matrix_effect} != 0 && {id_qmk_rgb_matrix_effect} != 24", v));
    }
    void keycodeLabels() {
        QCOMPARE(vki::keycodeLabel(0x04, {}), QString("A"));
        QCOMPARE(vki::keycodeLabel(0x5221, {}), QString("MO(1)"));
    }
};
QTEST_MAIN(CoreTests)
#include "core_tests.moc"
