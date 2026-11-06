#pragma once
#include "core/theme.h"
#include "device/definition.h"
#include "device/keycodes.h"

#include <QPolygonF>
#include <QTimer>
#include <QWidget>
#include <vector>

namespace vki {

class KeyboardView : public QWidget {
    Q_OBJECT
public:
    explicit KeyboardView(QWidget *parent = nullptr);
    void setDefinition(const Definition &definition);
    void setTheme(const Theme &theme);
    void setKeycodes(const QList<quint16> &layerCodes);  // rows * cols for one layer
    void setEnabledKeys(bool enabled);
    // Preview mode: one fill colour per key (definition order); not interactive.
    void setPreviewColors(const QList<QColor> &colors);
    void setSelectedKey(int row, int col);  // (-1, -1) clears
    // 3D mode: raised keycaps, hover lift, drag to orbit (software-projected, no GL).
    void set3D(bool on);
    const QList<KeyDef> &keys() const { return definition_.keys; }
    double layoutWidth() const { return definition_.width(); }
    QSize sizeHint() const override { return {900, 360}; }
    QSize minimumSizeHint() const override { return {520, 200}; }

signals:
    void keyClicked(int row, int col);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void focusOutEvent(QFocusEvent *) override { update(); }

private:
    Definition definition_;
    Theme theme_;
    QList<quint16> codes_;
    QList<QColor> preview_;
    bool previewMode_ = false, over_ = false;
    int selected_ = -1;
    int hover_ = -1, focused_ = 0;
    bool enabled_ = true;
    struct Face { int index; QPolygonF top, base; };
    bool threeD_ = false, pressing_ = false, dragging_ = false;
    QPointF pressPos_, lastPos_;
    double yaw_ = 0, pitch_ = .3, yawTarget_ = 0, pitchTarget_ = .3;
    QList<double> lift_;
    std::vector<Face> faces_;
    QPolygonF caseHull_;  // screen-space outline of the keyboard model, for hover
    QTimer *anim_ = nullptr;
    void paint3D();
    void stepAnimation();
    void kick();
    QRectF keyRect(const KeyDef &key) const;
    int keyAt(const QPointF &point) const;
};

}
