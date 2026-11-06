#include "keyboard_view.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QtMath>
#include <algorithm>

namespace vki {
namespace {
// Colourway accents sit on Esc, Enter, the space bar and the arrow cluster, by physical position.
bool accentKey(const KeyDef &k) {
    if (k.encoder >= 0) return false;
    if (k.x == 0 && k.y == 0) return true;
    if (k.w >= 5) return true;
    if (k.y == 3 && k.w > 2 && std::abs(k.x + k.w - 15) < .01) return true;
    return (k.y == 4 && std::abs(k.x - 14) < .01) || (k.y == 5 && k.w == 1 && k.x >= 12.99 && k.x <= 15.01);
}
QColor mixed(const QColor &a, const QColor &b, double t) {
    return QColor::fromRgbF(a.redF() * (1 - t) + b.redF() * t, a.greenF() * (1 - t) + b.greenF() * t,
                            a.blueF() * (1 - t) + b.blueF() * t, a.alphaF());
}
// Draws a legend centred in r, shrinking the type until it fits so nothing needs eliding.
void drawLegend(QPainter &p, QFont font, const QRectF &r, const QString &text, double singlePx, double minPx = 6) {
    if (text.isEmpty()) return;
    const QStringList lines = text.split('\n');
    double px = text.size() == 1 ? singlePx : (lines.size() > 1 ? singlePx * .8 : singlePx * .75);
    for (; px > minPx; px -= .5) {
        font.setPixelSize(std::max(1, int(std::round(px))));
        const QFontMetricsF fm(font);
        double widest = 0;
        for (const auto &line : lines) widest = std::max(widest, fm.horizontalAdvance(line));
        if (widest <= r.width() && fm.lineSpacing() * lines.size() <= r.height() + 2) break;
    }
    font.setPixelSize(std::max(1, int(std::round(px))));
    p.setFont(font);
    p.drawText(r, Qt::AlignCenter, text);
}
constexpr double kYawLimit = .1, kPitchMin = .12, kPitchMax = .45; }

KeyboardView::KeyboardView(QWidget *parent) : QWidget(parent), theme_(presetTheme("lofi")) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
}

void KeyboardView::setDefinition(const Definition &definition) {
    definition_ = definition;
    lift_.fill(0, definition_.keys.size());
    faces_.clear();
    focused_ = 0;
    selected_ = hover_ = -1;
    pressing_ = dragging_ = false;
    if (anim_) anim_->stop();
    update();
}

void KeyboardView::setTheme(const Theme &theme) {
    theme_ = theme;
    update();
}

void KeyboardView::setKeycodes(const QList<quint16> &codes) {
    codes_ = codes;
    update();
}

void KeyboardView::setPreviewColors(const QList<QColor> &colors) {
    preview_ = colors;
    previewMode_ = true;
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::ArrowCursor);
    hover_ = -1;
    update();
}

void KeyboardView::setSelectedKey(int row, int col) {
    selected_ = -1;
    for (int i = 0; i < definition_.keys.size(); ++i)
        if (definition_.keys[i].row == row && definition_.keys[i].col == col) selected_ = i;
    update();
}

void KeyboardView::setEnabledKeys(bool enabled) {
    enabled_ = enabled;
    setCursor(enabled ? Qt::PointingHandCursor : Qt::ArrowCursor);
    if (threeD_) kick();
    update();
}

QRectF KeyboardView::keyRect(const KeyDef &key) const {
    const double unit = std::min(width() / definition_.width(), height() / definition_.height());
    const double offsetX = (width() - unit * definition_.width()) / 2;
    const double offsetY = (height() - unit * definition_.height()) / 2;
    const double gap = std::max(2.0, unit * .06);
    return QRectF(offsetX + key.x * unit + gap / 2, offsetY + key.y * unit + gap / 2,
                  key.w * unit - gap, key.h * unit - gap);
}

int KeyboardView::keyAt(const QPointF &point) const {
    if (threeD_) {
        for (auto it = faces_.rbegin(); it != faces_.rend(); ++it)
            if (it->top.containsPoint(point, Qt::OddEvenFill) || it->base.containsPoint(point, Qt::OddEvenFill))
                return it->index;
        return -1;
    }
    for (int i = 0; i < definition_.keys.size(); ++i)
        if (keyRect(definition_.keys[i]).contains(point))
            return i;
    return -1;
}

void KeyboardView::paintEvent(QPaintEvent *) {
    if (definition_.keys.isEmpty())
        return;
    if (threeD_) { paint3D(); return; }
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const double unit = std::min(width() / definition_.width(), height() / definition_.height());
    QFont font = this->font();
    font.setPixelSize(std::max(8, int(unit * .26)));
    font.setWeight(QFont::Medium);
    p.setFont(font);
    for (int i = 0; i < definition_.keys.size(); ++i) {
        const auto &key = definition_.keys[i];
        const auto rect = keyRect(key);
        const bool hovered = !previewMode_ && enabled_ && i == hover_;
        const bool focused = !previewMode_ && hasFocus() && i == focused_;
        QColor fill = theme_.color("keycap");
        if (previewMode_ && i < preview_.size()) fill = preview_[i];
        const bool selected = !previewMode_ && i == selected_;
        if (hovered || selected) fill = theme_.color("selection");
        p.setPen(QPen(theme_.color(focused || hovered ? "accent" : "border"), focused ? 2 : 1));
        p.setBrush(fill);
        if (key.encoder >= 0) {
            const double d = std::min(rect.width(), rect.height());
            p.drawEllipse(QRectF(rect.center().x() - d / 2, rect.center().y() - d / 2, d, d));
        } else {
            p.drawRoundedRect(rect, unit * .12, unit * .12);
        }
        if (previewMode_) continue;
        const int index = key.row * definition_.cols + key.col;
        const quint16 code = index < codes_.size() ? codes_[index] : 0;
        p.setPen(theme_.color(enabled_ ? "keycap_text" : "muted"));
        drawLegend(p, font, rect.adjusted(3, 2, -3, -2), keycapLabel(code, definition_.customKeycodes), font.pixelSize());
    }
}

void KeyboardView::mouseMoveEvent(QMouseEvent *event) {
    if (pressing_ && !(event->buttons() & Qt::LeftButton)) {  // release was lost
        pressing_ = dragging_ = false;
        setCursor(enabled_ && !previewMode_ ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
    if (threeD_ && pressing_) {
        if (!dragging_ && (event->position() - pressPos_).manhattanLength() > 5) {
            dragging_ = true;
            setCursor(Qt::ClosedHandCursor);
            hover_ = -1;
        }
        if (dragging_) {
            const QPointF d = event->position() - lastPos_;
            yawTarget_ = std::clamp(yawTarget_ + d.x() * .004, -kYawLimit, kYawLimit);
            pitchTarget_ = std::clamp(pitchTarget_ + d.y() * .004, kPitchMin, kPitchMax);
            kick();
        }
        lastPos_ = event->position();
        return;
    }
    if (previewMode_) {
        const bool over = caseHull_.containsPoint(event->position(), Qt::WindingFill);
        if (over != over_) { over_ = over; update(); }
        return;
    }
    const int index = keyAt(event->position());
    if (index != hover_) {
        hover_ = index;
        kick();
        update();
    }
}

void KeyboardView::leaveEvent(QEvent *) {
    over_ = false;
    hover_ = -1;
    kick();
    update();
}

void KeyboardView::mousePressEvent(QMouseEvent *event) {
    if (previewMode_ && !threeD_) return;
    if (threeD_) {
        if (event->button() == Qt::LeftButton) {
            pressing_ = true;
            dragging_ = false;
            pressPos_ = lastPos_ = event->position();
            if (!previewMode_) setFocus();
        }
        return;
    }
    const int index = keyAt(event->position());
    if (index >= 0 && enabled_ && event->button() == Qt::LeftButton) {
        focused_ = index;
        setFocus();
        emit keyClicked(definition_.keys[index].row, definition_.keys[index].col);
    }
}

void KeyboardView::mouseReleaseEvent(QMouseEvent *event) {
    if (!threeD_ || !pressing_ || event->button() != Qt::LeftButton) return;
    const bool wasDrag = dragging_;
    pressing_ = dragging_ = false;
    yawTarget_ = 0;
    pitchTarget_ = .3;
    setCursor(enabled_ && !previewMode_ ? Qt::PointingHandCursor : Qt::ArrowCursor);
    if (previewMode_) { kick(); return; }
    if (!wasDrag && enabled_) {
        const int index = keyAt(event->position());
        if (index >= 0) {
            focused_ = index;
            emit keyClicked(definition_.keys[index].row, definition_.keys[index].col);
        } else {
            emit keyClicked(-1, -1);  // empty space clears the selection
        }
    }
    hover_ = keyAt(event->position());
    kick();
}

void KeyboardView::keyPressEvent(QKeyEvent *event) {
    if (previewMode_) { QWidget::keyPressEvent(event); return; }
    const int count = definition_.keys.size();
    if (!count) return;
    switch (event->key()) {
    case Qt::Key_Return: case Qt::Key_Enter: case Qt::Key_Space:
        if (enabled_) emit keyClicked(definition_.keys[focused_].row, definition_.keys[focused_].col);
        break;
    case Qt::Key_Right: case Qt::Key_Down: focused_ = (focused_ + 1) % count; update(); break;
    case Qt::Key_Left: case Qt::Key_Up: focused_ = (focused_ + count - 1) % count; update(); break;
    default: QWidget::keyPressEvent(event); return;
    }
    event->accept();
}


void KeyboardView::set3D(bool on) {
    if (threeD_ == on) return;
    threeD_ = on;
    lift_.fill(0, definition_.keys.size());
    if (on && !anim_) {
        anim_ = new QTimer(this);
        anim_->setInterval(16);
        connect(anim_, &QTimer::timeout, this, &KeyboardView::stepAnimation);
    }
    if (!on && anim_) anim_->stop();
    update();
}

void KeyboardView::kick() {
    if (threeD_ && anim_ && !anim_->isActive()) anim_->start();
}

void KeyboardView::stepAnimation() {
    bool moving = false;
    auto approach = [&moving](double &value, double target, double rate) {
        const double delta = target - value;
        if (std::abs(delta) < .002) { value = target; return; }
        value += delta * rate;
        moving = true;
    };
    approach(yaw_, yawTarget_, .18);
    approach(pitch_, pitchTarget_, .18);
    if (lift_.size() != definition_.keys.size()) lift_.fill(0, definition_.keys.size());
    for (int i = 0; i < lift_.size(); ++i)
        approach(lift_[i], i == hover_ && !dragging_ && enabled_ ? 1.0 : 0.0, .3);
    if (!moving && anim_) anim_->stop();
    update();
}

namespace {
constexpr double kUnit = 54, kGap = 3, kCapInset = 6, kCapHeight = 15, kLiftTravel = 10, kBaseZ = 3, kCaseDepth = 12, kCaseMargin = 16;
struct Pose {
    double cy, sy, cp, sp, dist, scale, ox, oy;
    struct P { double x, y, d; };
    P view(double x, double y, double z) const {
        const double x1 = x * cy - y * sy, y1 = x * sy + y * cy;
        return {x1, y1 * cp - z * sp, z * cp + y1 * sp};
    }
    QPointF screen(const P &p) const {
        const double f = dist / (dist - p.d);
        return {p.x * f * scale + ox, p.y * f * scale + oy};
    }
    QPointF project(double x, double y, double z) const { return screen(view(x, y, z)); }
    bool faces(double nx, double ny, double nz, const P &at) const {
        const P n = view(nx, ny, nz), o = view(0, 0, 0);
        const double vx = n.x - o.x, vy = n.y - o.y, vd = n.d - o.d;
        return vx * (0 - at.x) + vy * (0 - at.y) + vd * (dist - at.d) > 0;
    }
};
QColor shade(const QColor &c, double amount) {
    QColor out = c;
    const double k = 1 - amount;
    out.setRgbF(c.redF() * k, c.greenF() * k, c.blueF() * k, c.alphaF());
    return out;
}
}

void KeyboardView::paint3D() {
    const double W = definition_.width() * kUnit, H = definition_.height() * kUnit;
    Pose pose{};
    pose.dist = std::max(W, H) * 1.9;
    pose.scale = 1;
    auto setAngles = [&pose](double yaw, double pitch) {
        pose.cy = std::cos(yaw); pose.sy = std::sin(yaw);
        pose.cp = std::cos(pitch); pose.sp = std::sin(pitch);
    };
    // Fit using the resting pose and the widest tilt so dragging never clips.
    double minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
    const double ex = W / 2 + kCaseMargin, ey = H / 2 + kCaseMargin;
    for (const double pitch : {kPitchMin, kPitchMax})
        for (const double yaw : {-kYawLimit, kYawLimit}) {
            setAngles(yaw, pitch);
            for (const double sx : {-1., 1.})
                for (const double sy : {-1., 1.})
                    for (const double z : {-kCaseDepth, kBaseZ + kCapHeight + kLiftTravel}) {
                        const auto pt = pose.project(sx * ex, sy * ey, z);
                        minX = std::min(minX, pt.x()); maxX = std::max(maxX, pt.x());
                        minY = std::min(minY, pt.y()); maxY = std::max(maxY, pt.y());
                    }
        }
    const double pad = 4;
    pose.dist = std::max(pose.dist, 1.0);
    pose.scale = std::max(.05, std::min((width() - pad * 2) / (maxX - minX), (height() - pad * 2) / (maxY - minY)));
    // Centre on the resting pose's centre, not the union, so the board does not shift.
    setAngles(0, .3);
    pose.ox = pose.oy = 0;
    {
        double rx0 = 1e9, ry0 = 1e9, rx1 = -1e9, ry1 = -1e9;
        for (const double sx : {-1., 1.})
            for (const double sy : {-1., 1.})
                for (const double z : {-kCaseDepth, kBaseZ + kCapHeight}) {
                    const auto pt = pose.project(sx * ex, sy * ey, z);
                    rx0 = std::min(rx0, pt.x()); rx1 = std::max(rx1, pt.x());
                    ry0 = std::min(ry0, pt.y()); ry1 = std::max(ry1, pt.y());
                }
        pose.ox = width() / 2.0 - (rx0 + rx1) / 2;
        pose.oy = height() / 2.0 - (ry0 + ry1) / 2;
    }
    setAngles(yaw_, pitch_);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);

    const QColor caseColor = mixed(theme_.color("text"), theme_.color("background"), .08);
    const QColor plateColor = shade(caseColor, .3);
    auto quad = [&](double x0, double y0, double z0, double x1, double y1, double z1,
                    double x2, double y2, double z2, double x3, double y3, double z3) {
        return QPolygonF{pose.project(x0, y0, z0), pose.project(x1, y1, z1),
                         pose.project(x2, y2, z2), pose.project(x3, y3, z3)};
    };
    {  // outline of the whole case (convex hull of its corners) for the hover test
        std::vector<QPointF> pts;
        for (const double sx : {-1., 1.})
            for (const double sy : {-1., 1.})
                for (const double z : {-kCaseDepth, 0.})
                    pts.push_back(pose.project(sx * ex, sy * ey, z));
        std::sort(pts.begin(), pts.end(), [](const QPointF &a, const QPointF &b) { return a.x() < b.x() || (a.x() == b.x() && a.y() < b.y()); });
        auto cross = [](const QPointF &o, const QPointF &a, const QPointF &b) {
            return (a.x() - o.x()) * (b.y() - o.y()) - (a.y() - o.y()) * (b.x() - o.x());
        };
        std::vector<QPointF> hull(pts.size() * 2);
        size_t n = 0;
        for (size_t i = 0; i < pts.size(); ++i) {
            while (n >= 2 && cross(hull[n - 2], hull[n - 1], pts[i]) <= 0) --n;
            hull[n++] = pts[i];
        }
        for (size_t i = pts.size() - 1, t = n + 1; i > 0; --i) {
            while (n >= t && cross(hull[n - 2], hull[n - 1], pts[i - 1]) <= 0) --n;
            hull[n++] = pts[i - 1];
        }
        caseHull_.clear();
        for (size_t i = 0; i + 1 < n; ++i) caseHull_ << hull[i];
    }
    // Case body: visible outer walls, then the plate.
    {
        const double x0 = -ex, x1 = ex, y0 = -ey, y1 = ey, zt = 0, zb = -kCaseDepth;
        struct Wall { double nx, ny; double a[3], b[3], c[3], d[3]; double amount; };
        const Wall walls[] = {
            {0, 1, {x0, y1, zt}, {x1, y1, zt}, {x1, y1, zb}, {x0, y1, zb}, .12},
            {0, -1, {x1, y0, zt}, {x0, y0, zt}, {x0, y0, zb}, {x1, y0, zb}, .4},
            {-1, 0, {x0, y0, zt}, {x0, y1, zt}, {x0, y1, zb}, {x0, y0, zb}, .28},
            {1, 0, {x1, y1, zt}, {x1, y0, zt}, {x1, y0, zb}, {x1, y1, zb}, .28},
        };
        for (const auto &w : walls) {
            const auto mid = pose.view((w.a[0] + w.c[0]) / 2, (w.a[1] + w.c[1]) / 2, (w.a[2] + w.c[2]) / 2);
            if (!pose.faces(w.nx, w.ny, 0, mid)) continue;
            p.setBrush(shade(caseColor, w.amount));
            p.drawPolygon(quad(w.a[0], w.a[1], w.a[2], w.b[0], w.b[1], w.b[2], w.c[0], w.c[1], w.c[2], w.d[0], w.d[1], w.d[2]));
        }
        p.setBrush(caseColor);
        p.drawPolygon(quad(x0, y0, zt, x1, y0, zt, x1, y1, zt, x0, y1, zt));
        p.setBrush(plateColor);
        p.drawPolygon(quad(-W / 2 - 4, -H / 2 - 4, 0, W / 2 + 4, -H / 2 - 4, 0, W / 2 + 4, H / 2 + 4, 0, -W / 2 - 4, H / 2 + 4, 0));
    }

    const bool lit = previewMode_;
    if (lit) {  // backlight bleed on the plate on the plate around each key, as flat translucent rings
        for (int i = 0; i < definition_.keys.size() && i < preview_.size(); ++i) {
            const auto &k = definition_.keys[i];
            QColor glow = preview_[i];
            if (glow.alpha() == 0) continue;
            const double x0 = k.x * kUnit - W / 2 + kGap, y0 = k.y * kUnit - H / 2 + kGap;
            const double x1 = (k.x + k.w) * kUnit - W / 2 - kGap, y1 = (k.y + k.h) * kUnit - H / 2 - kGap;
            const double a = glow.alphaF();
            p.setPen(Qt::NoPen);
            // One soft radial falloff per key, drawn in the key's own plane: smooth edges, no blur pass.
            const double e = 20, hw = (x1 - x0) / 2, hh = (y1 - y0) / 2, cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
            const QPolygonF corners{pose.project(cx - hw - e, cy - hh - e, .3), pose.project(cx + hw + e, cy - hh - e, .3),
                                    pose.project(cx + hw + e, cy + hh + e, .3), pose.project(cx - hw - e, cy + hh + e, .3)};
            QTransform t;
            if (!QTransform::quadToQuad(QPolygonF(QRectF(-1, -1, 2, 2)), corners, t)) continue;
            const double inner = (hw / (hw + e) + hh / (hh + e)) / 2;
            QRadialGradient g(0, 0, 1);
            auto stop = [&](double at, double alpha) { QColor c = glow; c.setAlphaF(a * alpha); g.setColorAt(at, c); };
            stop(0, .75);
            stop(inner, .62);
            stop(inner + (1 - inner) * .35, .34);
            stop(inner + (1 - inner) * .7, .12);
            stop(1, 0);
            p.save();
            p.setTransform(t, true);
            p.setBrush(g);
            p.drawRect(QRectF(-1, -1, 2, 2));
            p.restore();
        }
    }

    struct Item { int index; double depth; };
    std::vector<Item> order;
    order.reserve(definition_.keys.size());
    if (lift_.size() != definition_.keys.size()) lift_.fill(0, definition_.keys.size());
    for (int i = 0; i < definition_.keys.size(); ++i) {
        const auto &k = definition_.keys[i];
        const double cx = (k.x + k.w / 2) * kUnit - W / 2, cy = (k.y + k.h / 2) * kUnit - H / 2;
        order.push_back({i, pose.view(cx, cy, kBaseZ + kCapHeight / 2).d});
    }
    std::stable_sort(order.begin(), order.end(), [](const Item &a, const Item &b) { return a.depth < b.depth; });

    faces_.clear();
    faces_.reserve(order.size());
    QFont font = this->font();
    font.setPixelSize(13);
    font.setWeight(QFont::Medium);
    p.setFont(font);
    const QFontMetrics metrics(font);

    for (const auto &item : order) {
        const int i = item.index;
        const auto &k = definition_.keys[i];
        const double lift = lift_[i];
        const double x0 = k.x * kUnit - W / 2 + kGap, y0 = k.y * kUnit - H / 2 + kGap;
        const double x1 = (k.x + k.w) * kUnit - W / 2 - kGap, y1 = (k.y + k.h) * kUnit - H / 2 - kGap;
        const double zb = kBaseZ + lift * kLiftTravel, zt = zb + kCapHeight;
        const double inset = std::min(kCapInset, std::min(x1 - x0, y1 - y0) / 4);
        const double tx0 = x0 + inset, ty0 = y0 + inset, tx1 = x1 - inset, ty1 = y1 - inset;
        const bool hovered = !lit && enabled_ && i == hover_, selected = !lit && i == selected_;
        const bool accent = accentKey(k);
        QColor top = accent ? theme_.color("accent") : mixed(theme_.color("text"), theme_.color("background"), .08);
        QColor legend = accent ? theme_.color("accent_text") : theme_.color("background");
        if (lit && over_ && i < preview_.size() && preview_[i].alpha() > 0) {  // light shows through on hover
            QColor led = preview_[i];
            const double a = led.alphaF();
            led.setAlpha(255);
            top = mixed(top, led, .45 * a);
        }
        if (hovered || selected) top = mixed(top, theme_.color("selection"), .3);
        if (!enabled_) legend = mixed(top, legend, .5);

        if (k.encoder >= 0) {
            const double kx = (x0 + x1) / 2, ky = (y0 + y1) / 2;
            const double radius = std::max(3.0, std::min(x1 - x0, y1 - y0) / 2 - 1), topRadius = std::max(1.5, radius - 3);
            const double knobTop = zb + kCapHeight + 12;
            constexpr int steps = 36;
            QPolygonF topRing, baseRing;
            for (int s = 0; s < steps; ++s) {
                const double a = 2 * M_PI * s / steps;
                topRing << pose.project(kx + topRadius * std::cos(a), ky + topRadius * std::sin(a), knobTop);
                baseRing << pose.project(kx + radius * std::cos(a), ky + radius * std::sin(a), zb);
            }
            p.setPen(Qt::NoPen);
            for (int s = 0; s < steps; ++s) {
                const double a0 = 2 * M_PI * s / steps, a1 = 2 * M_PI * (s + 1) / steps, am = (a0 + a1) / 2;
                const auto mid = pose.view(kx + (radius + topRadius) / 2 * std::cos(am), ky + (radius + topRadius) / 2 * std::sin(am), (zb + knobTop) / 2);
                if (!pose.faces(std::cos(am), std::sin(am), .3, mid)) continue;
                const QColor wall = shade(top, std::clamp(.25 + .15 * std::sin(am), .05, .45));
                p.setBrush(wall);
                p.setPen(QPen(wall, 1));
                const int n = (s + 1) % steps;
                p.drawPolygon(QPolygonF{baseRing[s], baseRing[n], topRing[n], topRing[s]});
            }
            p.setBrush(top);
            p.setPen(Qt::NoPen);
            p.drawPolygon(topRing);
            // A notch shows which way the knob faces and keeps it readable as a dial.
            p.setPen(QPen(shade(top, .35), 2, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(pose.project(kx, ky - topRadius * .35, knobTop), pose.project(kx, ky - topRadius * .85, knobTop));
            const int pressIndex = k.row * definition_.cols + k.col;
            const quint16 pressCode = pressIndex < codes_.size() ? codes_[pressIndex] : 0;
            const double half = topRadius * .5;
            QTransform t;
            const QPolygonF labelQuad{pose.project(kx - half, ky + topRadius * .1 - half * .2, knobTop),
                                      pose.project(kx + half, ky + topRadius * .1 - half * .2, knobTop),
                                      pose.project(kx + half, ky + topRadius * .1 + half * 1.8, knobTop),
                                      pose.project(kx - half, ky + topRadius * .1 + half * 1.8, knobTop)};
            if (QTransform::quadToQuad(QPolygonF(QRectF(0, 0, half * 2, half * 2)), labelQuad, t)) {
                p.save();
                p.setTransform(t, true);
                p.setPen(legend);
                drawLegend(p, font, QRectF(0, 0, half * 2, half * 2), keycapLabel(pressCode, definition_.customKeycodes), 13, 4);
                p.restore();
            }
            faces_.push_back(Face{i, topRing, baseRing});
            continue;
        }

        Face face{i, quad(tx0, ty0, zt, tx1, ty0, zt, tx1, ty1, zt, tx0, ty1, zt),
                  quad(x0, y0, zb, x1, y0, zb, x1, y1, zb, x0, y1, zb)};

        struct Side { double nx, ny; double a[3], b[3], c[3], d[3]; double amount; };
        const Side sides[] = {
            {0, 1, {x0, y1, zb}, {x1, y1, zb}, {tx1, ty1, zt}, {tx0, ty1, zt}, .1},
            {0, -1, {x1, y0, zb}, {x0, y0, zb}, {tx0, ty0, zt}, {tx1, ty0, zt}, .38},
            {-1, 0, {x0, y0, zb}, {x0, y1, zb}, {tx0, ty1, zt}, {tx0, ty0, zt}, .25},
            {1, 0, {x1, y1, zb}, {x1, y0, zb}, {tx1, ty0, zt}, {tx1, ty1, zt}, .25},
        };
        p.setPen(Qt::NoPen);
        for (const auto &s : sides) {
            const auto mid = pose.view((s.a[0] + s.c[0]) / 2, (s.a[1] + s.c[1]) / 2, (s.a[2] + s.c[2]) / 2);
            if (!pose.faces(s.nx, s.ny, .3, mid)) continue;
            p.setBrush(shade(top, s.amount));
            p.drawPolygon(quad(s.a[0], s.a[1], s.a[2], s.b[0], s.b[1], s.b[2], s.c[0], s.c[1], s.c[2], s.d[0], s.d[1], s.d[2]));
        }
        p.setBrush(top);
        p.setPen(Qt::NoPen);
        p.drawPolygon(face.top);

        const int index = k.row * definition_.cols + k.col;
        const quint16 code = index < codes_.size() ? codes_[index] : 0;
        const QRectF local(tx0, ty0, tx1 - tx0, ty1 - ty0);
        QTransform t;
        if (QTransform::quadToQuad(QPolygonF(local), face.top, t)) {
            p.save();
            p.setTransform(t, true);
            p.setPen(legend);
            drawLegend(p, font, local.adjusted(2, 1, -2, -1), keycapLabel(code, definition_.customKeycodes), 15);
            p.restore();
        }
        faces_.push_back(std::move(face));
    }
}

}
