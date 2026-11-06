#pragma once
#include "core/theme.h"
#include "device/definition.h"
#include "device/via.h"
#include "keyboard_view.h"

#include <QMap>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QWidget>

class QLabel;
class QPushButton;
class QScrollArea;

namespace vki {

class Modal;
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
    void setKeycodes(const QList<quint16> &codes) { preview_->setKeycodes(codes); }
    void matchPreview(QWidget *reference);
    void syncPreview();
    void setModal(Modal *modal) { modal_ = modal; }
    void setConfigDirectory(const QString &directory);
    // Re-applies the app accent to the board when "Match app accent" is on.
    void syncAccent();

protected:
    bool eventFilter(QObject *object, QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QWidget *topInset_ = nullptr;
    QPointer<QWidget> reference_;
    struct Row {
        Control control;
        QWidget *container = nullptr;
        QWidget *input = nullptr;
        QLabel *readout = nullptr;
        ColorSwatch *swatch = nullptr;
        QLabel *choiceText = nullptr;
        QLabel *choiceArrow = nullptr;
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
    QString directory_;
    int accentRow_ = -1;
    QPushButton *accentButton_ = nullptr;
    Modal *modal_ = nullptr;

    void build();
    void refreshPreview();
    void showColor(Row &row, int hue, int saturation);
    void apply(Row &row, int value, int second = -1);
    void showChoice(Row &row, int value);
    void openChoices(int index);
    void updateVisibility();
    void markDirty(int channel);
};

}
