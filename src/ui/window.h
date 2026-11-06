#pragma once
#include "core/theme.h"
#include "device/definition.h"

#include <QWidget>

class QLabel;
class QPushButton;
class QStackedWidget;

namespace vki {

class KeymapPage;
class LightingPage;
class Modal;
class ThemeWatcher;
class ViaDevice;

class Window : public QWidget {
    Q_OBJECT
public:
    Window(QString directory, Theme theme, Definition definition, QWidget *parent = nullptr);

private:
    QString directory_;
    Definition definition_;
    QWidget *content_;
    QLabel *banner_, *status_;
    QStackedWidget *pages_;
    QList<QPushButton *> nav_;
    LightingPage *lighting_;
    KeymapPage *keymap_;
    Modal *modal_;
    ThemeWatcher *watcher_;
    ViaDevice *device_;

    void applyTheme();
    void connectionChanged(bool connected);
    void select(int index);
};

}
