#include <algorithm>
#include "keymap_page.h"
#include <QShortcut>
#include <QMouseEvent>
#include "keyboard_view.h"
#include "modal.h"

#include <QDir>
#include <QEvent>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSaveFile>
#include <QTabBar>
#include <QVBoxLayout>

namespace vki {

// Bottom panel: category list on the left, clickable keycodes on the right.
class KeyPalette : public QWidget {
    Q_OBJECT
public:
    KeyPalette(const QList<KeycodeCategory> &categories, QWidget *parent = nullptr)
        : QWidget(parent), categories_(categories) {
        auto *root = new QHBoxLayout(this);
        root->setContentsMargins(14, 14, 14, 14);
        root->setSpacing(14);
        auto *side = new QVBoxLayout;
        side->setSpacing(2);
        for (int i = 0; i < categories_.size(); ++i) {
            auto *button = new QPushButton(categories_[i].name);
            button->setObjectName("category");
            button->setCheckable(true);
            button->setCursor(Qt::PointingHandCursor);
            button->setMinimumWidth(130);
            button->setMinimumHeight(button->fontMetrics().height() + 18);  // keeps descenders (y, g) from clipping
            button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            connect(button, &QPushButton::clicked, this, [this, i] { select(i); });
            side->addWidget(button);
            buttons_.append(button);
        }
        side->addStretch();
        root->addLayout(side);
        auto *right = new QVBoxLayout;
        right->setSpacing(10);
        scroll_ = new QScrollArea;
        scroll_->setWidgetResizable(true);
        scroll_->setFrameShape(QFrame::NoFrame);
        scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        grid_ = new QWidget;
        gridLayout_ = new QGridLayout(grid_);
        gridLayout_->setContentsMargins(0, 0, 8, 0);
        gridLayout_->setSpacing(8);
        scroll_->setWidget(grid_);
        right->addWidget(scroll_, 1);
        auto *hexRow = new QHBoxLayout;
        hexRow->addWidget(new QLabel("Raw keycode"));
        hex_ = new QLineEdit("0x0000");
        hex_->setMaximumWidth(120);
        hexRow->addWidget(hex_);
        auto *apply = new QPushButton("Apply");
        apply->setObjectName("secondary");
        apply->setCursor(Qt::PointingHandCursor);
        hexRow->addWidget(apply);
        hexRow->addStretch();
        right->addLayout(hexRow);
        root->addLayout(right, 1);
        connect(apply, &QPushButton::clicked, this, [this] {
            bool ok = false;
            const auto text = hex_->text().trimmed();
            const uint value = text.toUInt(&ok, text.startsWith("0x", Qt::CaseInsensitive) ? 16 : 0);
            if (ok && value <= 0xFFFF) emit picked(quint16(value));
        });
        int start = 0;
        for (int i = 0; i < categories_.size(); ++i)
            if (categories_[i].name == "Letters") start = i;
        scroll_->viewport()->installEventFilter(this);
        select(start);
    }
    void setCurrent(quint16 code) { hex_->setText(QString("0x%1").arg(code, 4, 16, QChar('0'))); }
signals:
    void picked(quint16 code);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched == scroll_->viewport() && event->type() == QEvent::Resize && columns() != columns_) fill();
        return QWidget::eventFilter(watched, event);
    }
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        if (columns() != columns_) fill();
    }

private:
    static constexpr int cellWidth = 78, cellHeight = 44, gap = 8;
    QList<KeycodeCategory> categories_;
    QList<QPushButton *> buttons_;
    QScrollArea *scroll_;
    QWidget *grid_;
    QGridLayout *gridLayout_;
    QLineEdit *hex_;
    int current_ = 0, columns_ = 0, rowsFilled_ = 0;

    int columns() const { return std::max(1, (scroll_->viewport()->width() - 8 + gap) / (cellWidth + gap)); }
    void select(int index) {
        current_ = index;
        for (int i = 0; i < buttons_.size(); ++i) buttons_[i]->setChecked(i == index);
        fill();
    }
    void fill() {
        while (auto *item = gridLayout_->takeAt(0)) {
            delete item->widget();  // never called from a key button's own signal
            delete item;
        }
        for (int i = 0; i <= columns_ + 1; ++i) gridLayout_->setColumnStretch(i, 0);
        for (int i = 0; i <= rowsFilled_ + 1; ++i) gridLayout_->setRowStretch(i, 0);
        columns_ = columns();
        if (current_ < 0 || current_ >= categories_.size()) return;
        const auto &entries = categories_[current_].entries;
        for (int i = 0; i < entries.size(); ++i) {
            auto *button = new QPushButton(entries[i].label);
            button->setObjectName("keyChoice");
            button->setCursor(Qt::PointingHandCursor);
            button->setToolTip(entries[i].label);
            button->setFixedSize(cellWidth, cellHeight);
            const quint16 code = entries[i].code;
            connect(button, &QPushButton::clicked, this, [this, code] { emit picked(code); });
            gridLayout_->addWidget(button, i / columns_, i % columns_);
        }
        rowsFilled_ = entries.size() / columns_ + 1;
        gridLayout_->setRowStretch(rowsFilled_, 1);
        gridLayout_->setColumnStretch(columns_, 1);
    }
};

KeymapPage::KeymapPage(const Definition &definition, ViaDevice *device, Modal *modal, QString configDirectory,
                       QWidget *parent)
    : QWidget(parent), definition_(definition), device_(device), modal_(modal),
      configDirectory_(std::move(configDirectory)), theme_(presetTheme("lofi")) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);
    layerBar_ = new QHBoxLayout;
    layerBar_->setSpacing(8);
    layout->addLayout(layerBar_);
    view_ = new KeyboardView;
    view_->setDefinition(definition_);
    view_->set3D(true);
    view_->setMinimumHeight(300);
    layout->addWidget(view_, 3);
    knobBar_ = new QWidget;
    auto *knobLayout = new QHBoxLayout(knobBar_);
    knobLayout->setContentsMargins(0, 0, 0, 0);
    knobLayout->setSpacing(8);
    auto *knobTitle = new QLabel("Knob");
    knobTitle->setObjectName("hint");
    knobLayout->addWidget(knobTitle);
    const char *const knobNames[] = {"Press", "Turn left", "Turn right"};
    for (int i = 0; i < 3; ++i) {
        auto *button = new QPushButton(knobNames[i]);
        button->setObjectName("layerButton");
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        connect(button, &QPushButton::clicked, this, [this, i] { setKnobTarget(i); });
        knobLayout->addWidget(button);
        knobButtons_.append(button);
    }
    knobLayout->addStretch();
    knobBar_->hide();
    layout->addWidget(knobBar_);
    hint_ = new QLabel;
    hint_->setObjectName("hint");
    hint_->setWordWrap(true);
    layout->addWidget(hint_);
    auto *panel = new QWidget;
    panel->setObjectName("card");
    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    palette_ = new KeyPalette(keycodeCategories(1, definition_.customKeycodes));
    panelLayout->addWidget(palette_);
    panel->setMinimumHeight(220);
    layout->addWidget(panel, 2);
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escape->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escape, &QShortcut::activated, this, [this] { selectKey(-1, -1); });
    connect(view_, &KeyboardView::keyClicked, this, &KeymapPage::selectKey);
    connect(palette_, &KeyPalette::picked, this, &KeymapPage::assign);
    updateHint();
}

void KeymapPage::mousePressEvent(QMouseEvent *event) {
    selectKey(-1, -1);  // a click on the page background clears the selection
    QWidget::mousePressEvent(event);
}

void KeymapPage::setTheme(const Theme &theme) {
    theme_ = theme;
    view_->setTheme(theme);
}

void KeymapPage::setAvailable(bool available) {
    view_->setEnabledKeys(available);
}

void KeymapPage::rebuildLayerButtons() {
    qDeleteAll(layerButtons_);
    layerButtons_.clear();
    while (auto *item = layerBar_->takeAt(0)) delete item;
    for (int i = 0; i < layers_; ++i) {
        auto *button = new QPushButton(QString("Layer %1").arg(i));
        button->setObjectName("layerButton");
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        connect(button, &QPushButton::clicked, this, [this, i] { showLayer(i); });
        layerBar_->addWidget(button);
        layerButtons_.append(button);
    }
    layerBar_->addStretch();
}

void KeymapPage::reload() {
    layers_ = device_->layerCount();
    keymap_ = device_->readKeymap();
    rebuildLayerButtons();
    if (keymap_.isEmpty()) {
        view_->setKeycodes({});
    emit keycodesChanged({});
        encoderCodes_.clear();
        encoderSupported_ = false;
        knobBar_->hide();
        view_->setEnabledKeys(false);
        updateHint();
        return;
    }
    {   // layer-switch keycodes depend on the real layer count
        auto *holder = palette_->parentWidget()->layout();
        holder->removeWidget(palette_);
        palette_->deleteLater();
        palette_ = new KeyPalette(keycodeCategories(layers_, definition_.customKeycodes));
        holder->addWidget(palette_);
        connect(palette_, &KeyPalette::picked, this, &KeymapPage::assign);
    }
    loadEncoders();
    backupOnce();
    showLayer(std::min(layer_, layers_ - 1));
    if (selRow_ >= 0) selectKey(selRow_, selCol_);
}

void KeymapPage::showLayer(int layer) {
    layer_ = layer;
    for (int i = 0; i < layerButtons_.size(); ++i) layerButtons_[i]->setChecked(i == layer);
    const int size = definition_.rows * definition_.cols;
    view_->setKeycodes(keymap_.mid(layer * size, size));
    emit keycodesChanged(keymap_.mid(layer * size, size));
    if (auto *code = selectedCode()) palette_->setCurrent(*code);
    updateHint();
}

void KeymapPage::selectKey(int row, int col) {
    selRow_ = row < 0 ? -1 : row;
    selCol_ = row < 0 ? -1 : col;
    view_->setSelectedKey(row, col);
    selEncoder_ = -1;
    for (const auto &key : definition_.keys)
        if (key.row == row && key.col == col) selEncoder_ = key.encoder;
    knobBar_->setVisible(selEncoder_ >= 0 && encoderSupported_);
    if (selEncoder_ < 0 || !encoderSupported_) knobTarget_ = 0;
    setKnobTarget(knobTarget_);
}

quint16 *KeymapPage::selectedCode() {
    if (selRow_ < 0) return nullptr;
    if (knobTarget_ > 0 && selEncoder_ >= 0) {
        auto it = encoderCodes_.find((layer_ * 64 + selEncoder_) * 2 + (knobTarget_ == 2 ? 1 : 0));
        return it == encoderCodes_.end() ? nullptr : &it.value();
    }
    const int index = (layer_ * definition_.rows + selRow_) * definition_.cols + selCol_;
    return index < keymap_.size() ? &keymap_[index] : nullptr;
}

void KeymapPage::setKnobTarget(int target) {
    knobTarget_ = target;
    for (int i = 0; i < knobButtons_.size(); ++i) knobButtons_[i]->setChecked(i == target);
    if (auto *code = selectedCode()) palette_->setCurrent(*code);
    updateHint();
}

void KeymapPage::loadEncoders() {
    encoderCodes_.clear();
    encoderSupported_ = false;
    if (device_->protocolVersion() < 10) return;
    for (const auto &key : definition_.keys) {
        if (key.encoder < 0) continue;
        for (int layer = 0; layer < layers_; ++layer)
            for (int cw = 0; cw < 2; ++cw) {
                const auto code = device_->encoderKeycode(layer, key.encoder, cw);
                if (!code) { encoderCodes_.clear(); encoderSupported_ = false; return; }
                encoderCodes_.insert((layer * 64 + key.encoder) * 2 + cw, *code);
            }
        encoderSupported_ = true;
    }
}

void KeymapPage::updateHint() {
    if (keymap_.isEmpty()) {
        hint_->setText("The keymap could not be read from the keyboard.");
    } else if (selRow_ < 0) {
        hint_->setText("Select a key above, then choose what it should send. Changes are written to the keyboard immediately.");
    } else {
        const auto *code = selectedCode();
        const QString what = knobTarget_ == 1 ? "turning the knob left" : knobTarget_ == 2 ? "turning the knob right"
                             : selEncoder_ >= 0 ? "pressing the knob" : "the selected key";
        hint_->setText(QString("Layer %1 · %2 sends %3. Choose a replacement below.")
                           .arg(layer_)
                           .arg(what)
                           .arg(code ? keycodeLabel(*code, definition_.customKeycodes) : "?"));
    }
}

void KeymapPage::assign(quint16 code) {
    if (selRow_ < 0) { hint_->setText("Select a key above first."); return; }
    auto *slot = selectedCode();
    if (!slot) return;
    const bool turn = knobTarget_ > 0 && selEncoder_ >= 0;
    const bool ok = turn ? device_->setEncoderKeycode(layer_, selEncoder_, knobTarget_ == 2, code)
                         : device_->setKeycode(layer_, selRow_, selCol_, code);
    if (ok) {
        *slot = code;
        showLayer(layer_);
        updateHint();
    } else {
        hint_->setText("The keyboard did not accept that change.");
    }
}

void KeymapPage::backupOnce() {
    const QString path = QDir(configDirectory_).filePath("keymap-backup.json");
    if (QFile::exists(path)) return;
    QJsonArray codes;
    for (auto code : keymap_) codes.append(int(code));
    QJsonObject root{{"device", definition_.name}, {"layers", layers_}, {"rows", definition_.rows},
                     {"cols", definition_.cols}, {"keycodes", codes}};
    QJsonObject knob;
    for (auto it = encoderCodes_.cbegin(); it != encoderCodes_.cend(); ++it) knob.insert(QString::number(it.key()), int(it.value()));
    if (!knob.isEmpty()) root.insert("encoders", knob);
    QDir().mkpath(configDirectory_);
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson());
        file.commit();
    }
}

}

#include "keymap_page.moc"

namespace vki {
QWidget *KeymapPage::keyboard() const { return view_; }
}
