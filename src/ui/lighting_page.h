#pragma once
#include "core/theme.h"
#include "device/definition.h"
#include "device/via.h"
#include "keyboard_view.h"

#include <QMap>
#include <QSet>
#include <QTimer>
#include <QWidget>

class QLabel;
class QScrollArea;

namespace vki {

class ColorPopover;
class ColorSwatch;

class LightingPage : public QWidget {
    Q_OBJECT
public:
    LightingPage(const Definition &definition, ViaDevice *device, QWidget *parent = nullptr);
    ~LightingPage() override;
    void setTheme(const Theme &theme);
    void reload();
    void setAvailable(bool available);

private:
    struct Row {
        Control control;
        QWidget *container = nullptr;
        QWidget *input = nullptr;
        QLabel *readout = nullptr;
        ColorSwatch *swatch = nullptr;
    };
    Definition definition_;
    ViaDevice *device_;
    QList<Row> rows_;
    QMap<QString, int> values_;
    QSet<int> dirtyChannels_;
    QTimer saveTimer_;
    bool loading_ = false;
    Theme theme_;
    KeyboardView *preview_ = nullptr;
    ColorPopover *popover_ = nullptr;
    QScrollArea *scroll_ = nullptr;
    int hue_ = 0, saturation_ = 255;
    int popoverRow_ = -1;

    void build();
    void refreshPreview();
    void showColor(Row &row, int hue, int saturation);
    void apply(Row &row, int value, int second = -1);
    void updateVisibility();
    void markDirty(int channel);
};

}
