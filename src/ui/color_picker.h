#pragma once
#include "core/theme.h"

#include <QAbstractButton>
#include <QElapsedTimer>
#include <QFrame>
#include <QImage>

class QLineEdit;

namespace vki {

// Hue / saturation values use the keyboard's own 0-255 ranges.
QColor deviceColor(int hue, int saturation);
void colorToDevice(const QColor &color, int &hue, int &saturation);

class ColorField : public QWidget {
    Q_OBJECT
public:
    explicit ColorField(QWidget *parent = nullptr);
    void setSelection(int hue, int saturation);
    QSize sizeHint() const override { return {220, 180}; }
    QSize minimumSizeHint() const override { return {160, 120}; }
signals:
    void changed(int hue, int saturation, bool final);
protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
private:
    QImage image_;
    int hue_ = 0, saturation_ = 255;
    QElapsedTimer throttle_;
    bool dragging_ = false;
    void pick(const QPointF &point, bool final);
};

class ColorSwatch : public QAbstractButton {
    Q_OBJECT
public:
    explicit ColorSwatch(QWidget *parent = nullptr);
    void setColor(const QColor &color) { color_ = color; update(); }
    void setTheme(const Theme &theme) { theme_ = theme; update(); }
    QSize sizeHint() const override { return {32, 32}; }
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QColor color_{Qt::white};
    Theme theme_;
};

// In-window popover (a child widget, not a separate window). Closes on outside click or Escape.
class ColorPopover : public QFrame {
    Q_OBJECT
public:
    explicit ColorPopover(QWidget *parent);
    ~ColorPopover() override;
    void setTheme(const Theme &theme);
    void setSelection(int hue, int saturation);
    void popup(const QPoint &topLeftInParent, QWidget *anchor);
signals:
    void changed(int hue, int saturation, bool final);
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
private:
    ColorField *field_;
    QLineEdit *hex_;
    QWidget *preview_;
    QWidget *anchor_ = nullptr;
    int hue_ = 0, saturation_ = 255;
    void refreshWidgets();
};

}
