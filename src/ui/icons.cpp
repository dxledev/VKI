#include "icons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QScreen>
#include <QGuiApplication>
#include <cmath>

namespace vki {

QIcon glyph(const QString &name, const QColor &color, int size) {
    const qreal dpr = QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->devicePixelRatio() : 1.0;
    QPixmap pixmap(QSize(size, size) * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 24.0, size / 24.0);
    QPen pen(color, 1.9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    if (name == "lighting") {
        p.drawEllipse(QPointF(12, 12), 4.2, 4.2);
        for (int i = 0; i < 8; ++i) {
            const qreal a = i * M_PI / 4;
            p.drawLine(QPointF(12 + std::cos(a) * 7.6, 12 + std::sin(a) * 7.6),
                       QPointF(12 + std::cos(a) * 10, 12 + std::sin(a) * 10));
        }
    } else if (name == "keymap") {
        p.drawRoundedRect(QRectF(2.5, 6, 19, 12), 2.5, 2.5);
        p.setPen(QPen(color, 2.2, Qt::SolidLine, Qt::RoundCap));
        for (qreal x : {6.5, 10.2, 13.8, 17.5}) p.drawPoint(QPointF(x, 10));
        p.drawLine(QPointF(7.5, 14), QPointF(16.5, 14));
    } else if (name == "palette") {
        QPainterPath path;
        path.moveTo(12, 3);
        path.cubicTo(5.5, 3, 2.5, 8, 3.5, 13);
        path.cubicTo(4.5, 18, 9, 21, 12.5, 20.5);
        path.cubicTo(15.5, 20, 14, 17, 15.5, 15.5);
        path.cubicTo(17, 14, 21, 15.5, 21, 11.5);
        path.cubicTo(21, 6.5, 17, 3, 12, 3);
        p.drawPath(path);
        p.setPen(QPen(color, 2.4, Qt::SolidLine, Qt::RoundCap));
        for (auto pt : {QPointF(8, 9), QPointF(12, 7), QPointF(16, 9), QPointF(7.5, 13.5)}) p.drawPoint(pt);
    } else if (name == "refresh") {
        QPainterPath arc;
        arc.arcMoveTo(QRectF(4, 4, 16, 16), 40);
        arc.arcTo(QRectF(4, 4, 16, 16), 40, 270);
        p.drawPath(arc);
        p.drawLine(QPointF(17.6, 3.5), QPointF(18.4, 8.4));
        p.drawLine(QPointF(18.4, 8.4), QPointF(13.6, 8.6));
    } else if (name == "usb") {
        p.drawLine(QPointF(12, 3), QPointF(12, 19));
        p.drawEllipse(QPointF(12, 20), 1.6, 1.6);
        p.drawLine(QPointF(12, 13), QPointF(7, 9.5));
        p.drawLine(QPointF(12, 11), QPointF(17, 7.5));
        p.drawRect(QRectF(5.2, 7, 3.6, 2.6));
        p.drawEllipse(QPointF(17.5, 7), 1.6, 1.6);
    }
    p.end();
    return QIcon(pixmap);
}

}
