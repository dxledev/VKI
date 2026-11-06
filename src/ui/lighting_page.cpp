#include "lighting_page.h"
#include "accent_sync.h"
#include "color_picker.h"
#include "icons.h"
#include "modal.h"
#include "themed_combo.h"

#include <QButtonGroup>
#include <QGridLayout>
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
    outer->setSpacing(0);
    topInset_ = new QWidget;
    topInset_->setFixedHeight(0);
    outer->addWidget(topInset_);
    preview_ = new KeyboardView;
    preview_->setDefinition(definition_);
    preview_->set3D(true);
    preview_->setMinimumHeight(300);
    preview_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    outer->addWidget(preview_);
    outer->addSpacing(14);
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
            } else if (control.type == Control::Dropdown) {
                auto *field = new QPushButton;
                field->setObjectName("choice");
                field->setCursor(Qt::PointingHandCursor);
                auto *inner = new QHBoxLayout(field);
                inner->setContentsMargins(12, 0, 10, 0);
                row.choiceText = new QLabel;
                row.choiceText->setAttribute(Qt::WA_TransparentForMouseEvents);
                row.choiceArrow = new QLabel;
                row.choiceArrow->setAttribute(Qt::WA_TransparentForMouseEvents);
                inner->addWidget(row.choiceText, 1);
                inner->addWidget(row.choiceArrow);
                row.input = field;
                connect(field, &QPushButton::clicked, this, [this, index] { openChoices(index); });
            } else if (control.type == Control::Toggle) {
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
                accent->setCheckable(true);
                accent->setCursor(Qt::PointingHandCursor);
                accentButton_ = accent;
                accentRow_ = index;
                connect(accent, &QPushButton::toggled, this, [this](bool on) {
                    if (directory_.isEmpty()) return;
                    setAccentSyncEnabled(directory_, on);
                    setAccentService(directory_, on);
                    if (on) syncAccent();
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

void LightingPage::setConfigDirectory(const QString &directory) {
    directory_ = directory;
    const bool on = accentSyncEnabled(directory_);
    if (accentButton_) {
        QSignalBlocker block(accentButton_);
        accentButton_->setChecked(on);
    }
    setAccentService(directory_, on);  // reconcile the background service with the saved choice
}

void LightingPage::syncAccent() {
    if (directory_.isEmpty() || accentRow_ < 0 || !accentSyncEnabled(directory_) || !device_->connected()) return;
    const auto c = theme_.color("accent");
    apply(rows_[accentRow_], std::clamp(c.hsvHue() < 0 ? 0 : c.hsvHue() * 256 / 360, 0, 255), c.hsvSaturation());
}

void LightingPage::setTheme(const Theme &theme) {
    const bool changed = theme.color("accent") != theme_.color("accent");
    theme_ = theme;
    if (changed) syncAccent();
    for (auto &row : rows_) {
        if (row.swatch) row.swatch->setTheme(theme);
        if (auto *combo = qobject_cast<ThemedCombo *>(row.input)) combo->setTheme(theme);
        if (row.choiceArrow) row.choiceArrow->setPixmap(glyph("chevron", theme.color("muted"), 16).pixmap(16, 16));
    }
    preview_->setTheme(theme);
    popover_->setTheme(theme);
    refreshPreview();
}

// Keep the preview exactly where and as large as the keymap page's keyboard.
void LightingPage::matchPreview(QWidget *reference) {
    reference_ = reference;
    reference->installEventFilter(this);
    syncPreview();
}

void LightingPage::syncPreview() {
    if (!reference_) return;
    if (auto *page = reference_->parentWidget(); page && page->layout() && !page->isVisible()) {
        page->setGeometry(rect());  // hidden pages are laid out lazily; do it now so the sizes agree
        page->layout()->invalidate();
        page->layout()->activate();
    }
    if (reference_->height() < 50) return;
    topInset_->setFixedHeight(std::max(0, reference_->y()));
    preview_->setFixedHeight(reference_->height());
}

void LightingPage::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    syncPreview();
    QTimer::singleShot(0, this, [this] { syncPreview(); });
}

void LightingPage::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    syncPreview();
}

bool LightingPage::eventFilter(QObject *object, QEvent *event) {
    if (object == reference_ && (event->type() == QEvent::Resize || event->type() == QEvent::Move ||
                                 event->type() == QEvent::Show))
        syncPreview();
    return QWidget::eventFilter(object, event);
}

void LightingPage::setAvailable(bool available) {
    // Only the board controls go dark; "Match app accent" must stay switchable (off never needs the board).
    for (auto &row : rows_)
        if (row.input) row.input->setEnabled(available);
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
    const double span = std::max(1.0, preview_->layoutWidth());
    QList<QColor> colors;
    colors.reserve(preview_->keys().size());
    for (const auto &key : preview_->keys()) {
        if (effect == 0) { colors.append(QColor(0, 0, 0, 0)); continue; }
        QColor lit = rainbow ? deviceColor(int((key.x + key.w / 2) / span * 255), 255) : deviceColor(hue_, saturation_);
        lit.setAlphaF(level <= 0 ? 0.0 : std::max(.15, level));
        colors.append(lit);
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
        } else if (row.choiceText) {
            showChoice(row, *value);
            row.readout->clear();
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
        if (row.choiceText) showChoice(row, value);
        if (c.type == Control::Range) row.readout->setText(QString::number(value));
    }
    values_[c.valueName] = value;
    if (ok) markDirty(c.channel);
    updateVisibility();
    refreshPreview();
}

void LightingPage::showChoice(Row &row, int value) {
    QString text = QString::number(value);
    for (const auto &option : row.control.options)
        if (option.second == value) text = option.first;
    row.choiceText->setText(text);
}

// Lists every option in an in-window modal; picking one applies it and closes.
void LightingPage::openChoices(int index) {
    if (!modal_ || index < 0 || index >= rows_.size() || modal_->isOpen()) return;
    const auto control = rows_[index].control;
    const int current = values_.value(control.valueName, -1);
    auto *content = new QWidget;
    auto *grid = new QGridLayout(content);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(8);
    const int columns = 3;
    auto *group = new QButtonGroup(content);
    auto *pending = new int(current);  // freed with the content
    connect(content, &QObject::destroyed, content, [pending] { delete pending; });
    for (int i = 0; i < control.options.size(); ++i) {
        const auto &option = control.options[i];
        auto *button = new QPushButton(option.first);
        button->setObjectName("choiceOption");
        button->setCheckable(true);
        button->setChecked(option.second == current);
        button->setCursor(Qt::PointingHandCursor);
        group->addButton(button);
        connect(button, &QPushButton::clicked, this, [this, pending, current, value = option.second] {
            *pending = value;
            modal_->setActionEnabled(0, value != current);
        });
        grid->addWidget(button, i / columns, i % columns);
    }
    for (int c = 0; c < columns; ++c) grid->setColumnStretch(c, 1);
    group->setExclusive(true);
    connect(modal_, &Modal::chosen, content, [this, index, pending](int action) {
        if (action == 0 && index < rows_.size()) apply(rows_[index], *pending);
    });
    modal_->openContent(control.label, content, {"Apply", "Cancel"}, true, true);
    modal_->setActionEnabled(0, false);
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
