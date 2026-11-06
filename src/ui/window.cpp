#include "window.h"
#include "icons.h"
#include "keymap_page.h"
#include "lighting_page.h"
#include "modal.h"
#include "theme_watcher.h"
#include "device/via.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace vki {
namespace {

QString css(QColor color) {
    return QString("rgba(%1,%2,%3,%4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(color.alpha());
}

const char *const navNames[] = {"keymap", "lighting"};
const char *const navLabels[] = {"Keymap", "Lighting"};

}

Window::Window(QString directory, Theme theme, Definition definition, QWidget *parent)
    : QWidget(parent), directory_(std::move(directory)), definition_(std::move(definition)) {
    setWindowTitle("VKI");
    setObjectName("vkiWindow");
    setAttribute(Qt::WA_TranslucentBackground);
    setMinimumSize(760, 520);
    resize(1180, 760);
    setFont(QFont("Noto Sans", 10));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    content_ = new QWidget(this);
    content_->setObjectName("content");
    root->addWidget(content_);
    auto *shell = new QHBoxLayout(content_);
    shell->setContentsMargins(0, 0, 0, 0);
    shell->setSpacing(0);

    auto *sidebar = new QWidget;
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(184);
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(14, 22, 14, 18);
    sideLayout->setSpacing(6);
    auto *brand = new QLabel("VKI");
    brand->setObjectName("brand");
    sideLayout->addWidget(brand);
    auto *model = new QLabel(definition_.name);
    model->setObjectName("hint");
    model->setWordWrap(true);
    sideLayout->addWidget(model);
    sideLayout->addSpacing(14);
    for (int i = 0; i < 2; ++i) {
        auto *button = new QPushButton(navLabels[i]);
        button->setObjectName("nav");
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setIconSize({20, 20});
        connect(button, &QPushButton::clicked, this, [this, i] { select(i); });
        sideLayout->addWidget(button);
        nav_.append(button);
    }
    sideLayout->addStretch();
    status_ = new QLabel;
    status_->setObjectName("status");
    status_->setWordWrap(true);
    sideLayout->addWidget(status_);
    shell->addWidget(sidebar);

    auto *main = new QWidget;
    auto *mainLayout = new QVBoxLayout(main);
    mainLayout->setContentsMargins(28, 24, 28, 24);
    mainLayout->setSpacing(14);
    banner_ = new QLabel;
    banner_->setObjectName("banner");
    banner_->setWordWrap(true);
    mainLayout->addWidget(banner_);
    pages_ = new QStackedWidget;
    mainLayout->addWidget(pages_, 1);
    shell->addWidget(main, 1);

    modal_ = new Modal(this, content_);
    device_ = new ViaDevice(definition_, this);
    lighting_ = new LightingPage(definition_, device_);
    keymap_ = new KeymapPage(definition_, device_, modal_, directory_);
    pages_->addWidget(keymap_);
    pages_->addWidget(lighting_);

    watcher_ = new ThemeWatcher(directory_ + "/theme.json", std::move(theme), this);
    connect(watcher_, &ThemeWatcher::changed, this, &Window::applyTheme);
    connect(watcher_, &ThemeWatcher::rejected, this, [this](const QString &message) {
        status_->setText("Theme not applied: " + message);
    });
    connect(device_, &ViaDevice::connectionChanged, this, &Window::connectionChanged);
    applyTheme();
    select(qBound(0, qEnvironmentVariableIntValue("VKI_PAGE"), 1));
    connectionChanged(device_->connected());
}

void Window::select(int index) {
    pages_->setCurrentIndex(index);
    for (int i = 0; i < nav_.size(); ++i) nav_[i]->setChecked(i == index);
}

void Window::connectionChanged(bool connected) {
    lighting_->setAvailable(connected);
    keymap_->setAvailable(connected);
    if (connected) {
        banner_->hide();
        status_->setText("Connected");
        lighting_->reload();
        keymap_->reload();
    } else {
        banner_->setText("Keyboard not found. Connect it with the USB cable and set it to wired mode. "
                         "If it is plugged in, your account may need read and write access to its hidraw node "
                         "(see the udev rule in the README).");
        banner_->show();
        status_->setText("Not connected");
    }
}

void Window::applyTheme() {
    const auto &theme = watcher_->theme();
    auto c = [&](const char *key) { return css(theme.color(key)); };
    setStyleSheet(
        QString("QWidget#vkiWindow {background:transparent;} "
                "QWidget#content {background:%1;} "
                "QScrollArea {background:transparent;border:none;} "
                "QScrollArea > QWidget > QWidget {background:transparent;} "
                "QWidget {color:%2;font-family:'Noto Sans';font-size:13px;} "
                "QLabel {background:transparent;} "
                "QWidget#sidebar {background:%4;} "
                "QLabel#brand {font-size:26px;font-weight:600;} "
                "QLabel#hint, QLabel#status, QLabel#readout {color:%3;font-size:12px;} "
                "QLabel#cardTitle {font-size:16px;font-weight:600;} "
                "QLabel#code {color:%3;font-family:monospace;font-size:12px;} "
                "QLabel#banner {background:%4;border:1px solid %9;border-radius:10px;padding:12px 14px;} "
                "QWidget#card {background:%4;border:1px solid %5;border-radius:14px;} "
                "QWidget#card QLabel {border:none;} "
                "QPushButton {background:%4;border:1px solid %5;border-radius:8px;padding:8px 14px;} "
                "QPushButton:hover {background:%6;border-color:%7;} "
                "QPushButton:focus {border:2px solid %7;padding:7px 13px;} "
                "QPushButton:checked {background:%7;color:%8;border-color:%7;} "
                "QPushButton#nav {background:transparent;border:none;text-align:left;padding:10px 12px;} "
                "QPushButton#nav:hover {background:%6;} "
                "QPushButton#nav:checked {background:%10;color:%2;} "
                "QPushButton#secondary {padding:6px 12px;} "
                "QPushButton#keyChoice {padding:4px 4px;font-size:12px;} "
                "QPushButton#category {background:transparent;border:none;text-align:left;padding:8px 12px;color:%3;} "
                "QPushButton#category:hover {background:%6;color:%2;} "
                "QPushButton#category:checked {background:%10;color:%2;} "
                "QPushButton#layerButton:checked {background:%7;color:%8;} "
                "QLineEdit {background:%1;border:1px solid %5;border-radius:7px;padding:6px 8px;} "
                "QLineEdit:focus {border-color:%7;} "
                "QSlider::groove:horizontal {height:6px;background:%6;border-radius:3px;} "
                "QSlider::sub-page:horizontal {background:%7;border-radius:3px;} "
                "QSlider::handle:horizontal {background:%2;width:16px;height:16px;margin:-5px 0;border-radius:8px;} "
                "QScrollBar:vertical {background:transparent;width:10px;margin:0;} "
                "QScrollBar::handle:vertical {background:%5;border-radius:5px;min-height:30px;} "
                "QScrollBar::add-line, QScrollBar::sub-line {height:0;} "
                "QScrollBar::add-page, QScrollBar::sub-page {background:transparent;} "
                "QTabBar::tab {background:transparent;color:%3;padding:7px 12px;border:none;"
                "border-bottom:2px solid transparent;font-size:12px;} "
                "QTabBar::tab:selected {color:%2;border-bottom-color:%7;} "
                "QTabBar::tab:hover {color:%2;} QTabBar:focus {outline:none;} "
                "QTabBar QToolButton {background:%4;border:none;border-radius:4px;color:%2;} "
                "QTabBar QToolButton:hover {background:%6;} "
                "QWidget#modalCard {background:%4;border:1px solid %5;border-radius:14px;} "
                "QWidget#modalCard QLabel {border:none;} "
                "QLabel#modalTitle {font-size:22px;font-weight:600;} "
                "QWidget#modalCard QPushButton {min-height:20px;}")
            .arg(c("background"), c("text"), c("muted"), c("surface"), c("border"), c("surface_alt"), c("accent"),
                 c("accent_text"), c("danger"), c("selection")));
    for (int i = 0; i < nav_.size(); ++i)
        nav_[i]->setIcon(glyph(navNames[i], theme.color("text")));
    lighting_->setTheme(theme);
    keymap_->setTheme(theme);
    modal_->setTheme(theme);
    setWindowIcon(QIcon::fromTheme("io.github.vki"));
}

}
