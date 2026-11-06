#include "color_picker.h"

#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>
#include <algorithm>

namespace vki {

QColor deviceColor(int hue, int saturation) {
    return QColor::fromHsv(std::clamp(hue * 360 / 256, 0, 359), std::clamp(saturation, 0, 255), 255);
}

void colorToDevice(const QColor &color, int &hue, int &saturation) {
    const int h = color.hsvHue();
    hue = std::clamp(h < 0 ? 0 : h * 256 / 360, 0, 255);
    saturation = std::clamp(color.hsvSaturation(), 0, 255);
}

ColorField::ColorField(QWidget *parent) : QWidget(parent) {
    setCursor(Qt::CrossCursor);
}

void ColorField::setSelection(int hue, int saturation) {
    hue_ = std::clamp(hue, 0, 255);
    saturation_ = std::clamp(saturation, 0, 255);
    update();
}

void ColorField::resizeEvent(QResizeEvent *) {
    const int w = std::max(2, width()), h = std::max(2, height());
    image_ = QImage(w, h, QImage::Format_RGB32);
    for (int y = 0; y < h; ++y) {
        auto *line = reinterpret_cast<QRgb *>(image_.scanLine(y));
        const int saturation = 255 - y * 255 / (h - 1);
        for (int x = 0; x < w; ++x) line[x] = deviceColor(x * 255 / (w - 1), saturation).rgb();
    }
}

void ColorField::paintEvent(QPaintEvent *) {
    QPainter p(this);
    if (!image_.isNull()) p.drawImage(0, 0, image_);
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF center(hue_ * (width() - 1) / 255.0, (255 - saturation_) * (height() - 1) / 255.0);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(Qt::black, 3));
    p.drawEllipse(center, 6, 6);
    p.setPen(QPen(Qt::white, 1.5));
    p.drawEllipse(center, 6, 6);
}

void ColorField::pick(const QPointF &point, bool final) {
    const double x = std::clamp(point.x(), 0.0, double(width() - 1));
    const double y = std::clamp(point.y(), 0.0, double(height() - 1));
    hue_ = int(x * 255 / std::max(1, width() - 1));
    saturation_ = 255 - int(y * 255 / std::max(1, height() - 1));
    update();
    emit changed(hue_, saturation_, final);
}

void ColorField::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) return;
    dragging_ = true;
    throttle_.start();
    pick(event->position(), false);
}

void ColorField::mouseMoveEvent(QMouseEvent *event) {
    if (!dragging_ || !(event->buttons() & Qt::LeftButton)) return;
    if (throttle_.elapsed() < 40) {  // keep the HID write rate sane while dragging
        const double x = std::clamp(event->position().x(), 0.0, double(width() - 1));
        const double y = std::clamp(event->position().y(), 0.0, double(height() - 1));
        hue_ = int(x * 255 / std::max(1, width() - 1));
        saturation_ = 255 - int(y * 255 / std::max(1, height() - 1));
        update();
        return;
    }
    throttle_.restart();
    pick(event->position(), false);
}

void ColorField::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton || !dragging_) return;
    dragging_ = false;
    pick(event->position(), true);
}

ColorSwatch::ColorSwatch(QWidget *parent) : QAbstractButton(parent), theme_(presetTheme("lofi")) {
    setCursor(Qt::PointingHandCursor);
    setFixedSize(32, 32);
}

void ColorSwatch::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(color_);
    p.setPen(QPen(theme_.color(hasFocus() ? "accent" : "border"), 2));
    p.drawEllipse(QRectF(rect()).adjusted(2, 2, -2, -2));
}

ColorPopover::ColorPopover(QWidget *parent) : QFrame(parent) {
    setObjectName("colorPopover");
    setAttribute(Qt::WA_StyledBackground, true);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);
    hex_ = new QLineEdit;
    hex_->setAlignment(Qt::AlignCenter);
    hex_->setMaxLength(7);
    layout->addWidget(hex_);
    preview_ = new QWidget;
    preview_->setFixedHeight(18);
    preview_->setAttribute(Qt::WA_StyledBackground, true);
    layout->addWidget(preview_);
    field_ = new ColorField;
    layout->addWidget(field_);
    connect(field_, &ColorField::changed, this, [this](int hue, int saturation, bool final) {
        hue_ = hue;
        saturation_ = saturation;
        refreshWidgets();
        emit changed(hue, saturation, final);
    });
    connect(hex_, &QLineEdit::editingFinished, this, [this] {
        QColor c(hex_->text().trimmed());
        if (!c.isValid()) { refreshWidgets(); return; }
        colorToDevice(c, hue_, saturation_);
        refreshWidgets();
        emit changed(hue_, saturation_, true);
    });
    setFixedWidth(240);
    hide();
}

ColorPopover::~ColorPopover() {
    qApp->removeEventFilter(this);
}

void ColorPopover::setTheme(const Theme &theme) {
    const auto css = QString("#colorPopover {background:%1;border:1px solid %2;border-radius:10px;} "
                             "QLineEdit {background:%3;border:1px solid %2;border-radius:6px;padding:5px;color:%4;}")
                         .arg(theme.color("surface").name(QColor::HexArgb), theme.color("border").name(QColor::HexArgb),
                              theme.color("surface_alt").name(QColor::HexArgb), theme.color("text").name(QColor::HexArgb));
    setStyleSheet(css);
    refreshWidgets();
}

void ColorPopover::setSelection(int hue, int saturation) {
    hue_ = hue;
    saturation_ = saturation;
    field_->setSelection(hue, saturation);
    refreshWidgets();
}

void ColorPopover::refreshWidgets() {
    const QColor c = deviceColor(hue_, saturation_);
    hex_->setText(c.name());
    preview_->setStyleSheet(QString("background:%1;border-radius:4px;").arg(c.name()));
    field_->setSelection(hue_, saturation_);
}

void ColorPopover::popup(const QPoint &topLeftInParent, QWidget *anchor) {
    anchor_ = anchor;
    adjustSize();
    auto *host = parentWidget();
    QPoint pos = topLeftInParent;
    pos.setX(std::clamp(pos.x(), 4, std::max(4, host->width() - width() - 4)));
    pos.setY(std::clamp(pos.y(), 4, std::max(4, host->height() - height() - 4)));
    move(pos);
    raise();
    show();
    hex_->setFocus();
}

void ColorPopover::showEvent(QShowEvent *) {
    qApp->removeEventFilter(this);
    qApp->installEventFilter(this);
}

void ColorPopover::hideEvent(QHideEvent *) {
    qApp->removeEventFilter(this);
}

void ColorPopover::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) hide();
    else QFrame::keyPressEvent(event);
}

bool ColorPopover::eventFilter(QObject *, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress) {
        const auto global = static_cast<QMouseEvent *>(event)->globalPosition().toPoint();
        const QRect mine(mapToGlobal(QPoint(0, 0)), size());
        bool onAnchor = false;
        if (anchor_) onAnchor = QRect(anchor_->mapToGlobal(QPoint(0, 0)), anchor_->size()).contains(global);
        if (!mine.contains(global) && !onAnchor) hide();
    }
    return false;
}

}
