#include "lighting_page.h"
#include "color_picker.h"
#include "themed_combo.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>

namespace vki {
LightingPage::LightingPage(const Definition &definition, ViaDevice *device, QWidget *parent)
    : QWidget(parent), definition_(definition), device_(device), theme_(presetTheme("lofi")) {
    saveTimer_.setSingleShot(true);
    saveTimer_.setInterval(600);
    connect(&saveTimer_, &QTimer::timeout, this, [this] {
        for (int channel : std::as_const(dirtyChannels_))
            device_->saveChannel(channel);
        dirtyChannels_.clear();
    });
    build();
}

LightingPage::~LightingPage() {
    for (int channel : std::as_const(dirtyChannels_)) device_->saveChannel(channel);  // flush pending writes
}

void LightingPage::build() {
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(14);
    preview_ = new KeyboardView;
    preview_->setDefinition(definition_);
    preview_->setMinimumHeight(150);
    preview_->setMaximumHeight(300);
    preview_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    outer->addWidget(preview_);
    auto *scroll = new QScrollArea(this);
    scroll_ = scroll;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setAutoFillBackground(false);
    outer->addWidget(scroll, 1);
    auto *body = new QWidget;
    body->setAutoFillBackground(false);
    auto *column = new QVBoxLayout(body);
    column->setContentsMargins(0, 0, 8, 0);
    column->setSpacing(16);
    for (const auto &section : definition_.sections) {
        auto *card = new QWidget;
        card->setObjectName("card");
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(22, 20, 22, 22);
        cardLayout->setSpacing(14);
        auto *title = new QLabel(section.label);
        title->setObjectName("cardTitle");
        cardLayout->addWidget(title);
        for (const auto &control : section.controls) {
            Row row;
            row.control = control;
            row.container = new QWidget;
            auto *layout = new QVBoxLayout(row.container);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(6);
            auto *head = new QHBoxLayout;
            auto *label = new QLabel(control.label);
            head->addWidget(label);
            head->addStretch();
            row.readout = new QLabel;
            row.readout->setObjectName("readout");
            head->addWidget(row.readout);
            layout->addLayout(head);
            const int index = rows_.size();
            if (control.type == Control::Range) {
                auto *slider = new QSlider(Qt::Horizontal);
                slider->setRange(control.min, control.max);
                slider->setFocusPolicy(Qt::StrongFocus);
                row.input = slider;
                connect(slider, &QSlider::valueChanged, this, [this, index](int value) {
                    if (!loading_) apply(rows_[index], value);
                });
            } else if (control.type == Control::Dropdown || control.type == Control::Toggle) {
                auto *combo = new ThemedCombo;
                if (control.type == Control::Toggle) {
                    combo->addItem("Off", 0);
                    combo->addItem("On", 1);
                } else {
                    for (const auto &option : control.options) combo->addItem(option.first, option.second);
                }
                row.input = combo;
                connect(combo, &QComboBox::activated, this, [this, index, combo](int i) {
                    if (!loading_) apply(rows_[index], combo->itemData(i).toInt());
                });
            } else {
                row.swatch = new ColorSwatch;
                row.input = row.swatch;
                connect(row.swatch, &QAbstractButton::clicked, this, [this, index] {
                    if (popover_->isVisible()) { popover_->hide(); return; }
                    popoverRow_ = index;
                    auto *button = rows_[index].swatch;
                    popover_->setSelection(hue_, saturation_);
                    popover_->popup(button->mapTo(this, QPoint(button->width() - popover_->width(), button->height() + 6)),
                                    button);
                });
                auto *accent = new QPushButton("Match app accent");
                accent->setObjectName("secondary");
                accent->setCursor(Qt::PointingHandCursor);
                connect(accent, &QPushButton::clicked, this, [this, index] {
                    const auto c = theme_.color("accent");
                    apply(rows_[index], std::clamp(c.hsvHue() < 0 ? 0 : c.hsvHue() * 256 / 360, 0, 255),
                          c.hsvSaturation());
                });
                auto *foot = new QHBoxLayout;
                foot->addWidget(row.input);
                foot->addWidget(accent);
                foot->addStretch();
                layout->addLayout(foot);
                cardLayout->addWidget(row.container);
                rows_.append(row);
                continue;
            }
            layout->addWidget(row.input);
            cardLayout->addWidget(row.container);
            rows_.append(row);
        }
        column->addWidget(card);
    }
    column->addStretch();
    scroll->setWidget(body);
    popover_ = new ColorPopover(this);
    connect(popover_, &ColorPopover::changed, this, [this](int hue, int saturation, bool) {
        if (loading_) return;
        if (popoverRow_ >= 0 && popoverRow_ < rows_.size()) apply(rows_[popoverRow_], hue, saturation);
    });
    connect(scroll->verticalScrollBar(), &QScrollBar::valueChanged, popover_, &QWidget::hide);
}

void LightingPage::setTheme(const Theme &theme) {
    theme_ = theme;
    for (auto &row : rows_) {
        if (row.swatch) row.swatch->setTheme(theme);
        if (auto *combo = qobject_cast<ThemedCombo *>(row.input)) combo->setTheme(theme);
    }
    preview_->setTheme(theme);
    popover_->setTheme(theme);
    refreshPreview();
}

void LightingPage::setAvailable(bool available) {
    setEnabled(available);
    if (!available) popover_->hide();
}

void LightingPage::showColor(Row &row, int hue, int saturation) {
    hue_ = hue;
    saturation_ = saturation;
    row.swatch->setColor(deviceColor(hue, saturation));
    row.readout->setText(deviceColor(hue, saturation).name());
}

// Approximation of the board's RGB matrix: static colour for most effects, a hue sweep for the
// rainbow/cycle family, dark when off. Brightness follows the brightness slider.
void LightingPage::refreshPreview() {
    int effect = 1, brightness = 200, maxBrightness = 200;
    for (const auto &row : rows_) {
        if (row.control.channel != 3) continue;
        if (row.control.id == 2) effect = values_.value(row.control.valueName, 1);
        if (row.control.id == 1) {
            brightness = values_.value(row.control.valueName, 200);
            maxBrightness = std::max(1, row.control.max);
            if (auto *slider = qobject_cast<QSlider *>(row.input)) maxBrightness = std::max(1, slider->maximum());
        }
    }
    const bool rainbow = (effect >= 12 && effect <= 23) || effect == 25 || effect == 26 || effect == 27 || effect == 28;
    const double level = std::clamp(double(brightness) / maxBrightness, 0.0, 1.0);
    const QColor base = theme_.color("keycap");
    const double span = std::max(1.0, preview_->layoutWidth());
    QList<QColor> colors;
    colors.reserve(preview_->keys().size());
    for (const auto &key : preview_->keys()) {
        if (effect == 0) { colors.append(base); continue; }
        QColor lit = rainbow ? deviceColor(int((key.x + key.w / 2) / span * 255), 255) : deviceColor(hue_, saturation_);
        const auto mix = [&](int a, int b) { return int(a + (b - a) * level); };
        colors.append(QColor(mix(base.red(), lit.red()), mix(base.green(), lit.green()), mix(base.blue(), lit.blue())));
    }
    preview_->setPreviewColors(colors);
}

void LightingPage::reload() {
    loading_ = true;
    values_.clear();
    for (auto &row : rows_) {
        const auto &c = row.control;
        if (c.type == Control::Color) {
            if (auto pair = device_->getPair(c.channel, c.id)) {
                values_[c.valueName] = pair->first;
                showColor(row, pair->first, pair->second);
            }
            continue;
        }
        const auto value = device_->getValue(c.channel, c.id);
        if (!value) continue;
        values_[c.valueName] = *value;
        if (auto *slider = qobject_cast<QSlider *>(row.input)) {
            slider->setMaximum(std::max(c.max, *value));
            slider->setValue(*value);
            row.readout->setText(QString::number(*value));
        } else if (auto *combo = qobject_cast<ThemedCombo *>(row.input)) {
            const int index = combo->findData(*value);
            combo->setCurrentIndex(std::max(0, index));
            row.readout->clear();
        }
    }
    loading_ = false;
    updateVisibility();
    refreshPreview();
}

void LightingPage::apply(Row &row, int value, int second) {
    const auto &c = row.control;
    bool ok;
    if (c.type == Control::Color) {
        ok = device_->setPair(c.channel, c.id, value, second);
        showColor(row, value, second);
    } else {
        ok = device_->setValue(c.channel, c.id, value);
        if (c.type == Control::Range) row.readout->setText(QString::number(value));
    }
    values_[c.valueName] = value;
    if (ok) markDirty(c.channel);
    updateVisibility();
    refreshPreview();
}

void LightingPage::updateVisibility() {
    for (auto &row : rows_)
    {
        const bool show = evaluateShowIf(row.control.showIf, values_);
        row.container->setVisible(show);
        if (!show && row.swatch) popover_->hide();
    }
}

void LightingPage::markDirty(int channel) {
    dirtyChannels_.insert(channel);
    saveTimer_.start();
}

}
