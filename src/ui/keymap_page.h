#pragma once
#include "core/theme.h"
#include "device/definition.h"
#include "device/via.h"

#include <QHash>
#include <QWidget>

class QLabel;
class QMouseEvent;
class QHBoxLayout;
class QPushButton;

namespace vki {

class KeyboardView;
class KeyPalette;
class Modal;

class KeymapPage : public QWidget {
    Q_OBJECT
public:
    KeymapPage(const Definition &definition, ViaDevice *device, Modal *modal, QString configDirectory,
               QWidget *parent = nullptr);
    void setTheme(const Theme &theme);
    void reload();
    void setAvailable(bool available);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    Definition definition_;
    ViaDevice *device_;
    Modal *modal_;
    QString configDirectory_;
    KeyboardView *view_;
    KeyPalette *palette_;
    int selRow_ = -1, selCol_ = -1;
    QLabel *hint_;
    QHBoxLayout *layerBar_;
    QList<QPushButton *> layerButtons_;
    QList<quint16> keymap_;
    int layer_ = 0, layers_ = 1;
    QWidget *knobBar_ = nullptr;
    QList<QPushButton *> knobButtons_;
    int knobTarget_ = 0;        // 0 press, 1 turn left, 2 turn right
    int selEncoder_ = -1;
    QHash<int, quint16> encoderCodes_;  // key: (layer * 64 + encoder) * 2 + clockwise
    bool encoderSupported_ = false;
    void setKnobTarget(int target);
    void loadEncoders();
    quint16 *selectedCode();
    Theme theme_;
    void showLayer(int layer);
    void rebuildLayerButtons();
    void selectKey(int row, int col);
    void assign(quint16 code);
    void updateHint();
    void backupOnce();
};

}
