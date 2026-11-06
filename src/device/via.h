#pragma once
#include "definition.h"

#include <QByteArray>
#include <QObject>
#include <QTimer>
#include <optional>

namespace vki {

class ViaDevice : public QObject {
    Q_OBJECT
public:
    explicit ViaDevice(const Definition &definition, QObject *parent = nullptr);
    ~ViaDevice() override;

    bool connected() const { return fd_ >= 0; }
    QString nodePath() const { return node_; }
    int protocolVersion() const { return protocol_; }
    int layerCount() const { return layers_; }

    // Each call is synchronous and returns false when the keyboard did not answer.
    std::optional<int> getValue(int channel, int id);
    std::optional<QPair<int, int>> getPair(int channel, int id);
    bool setValue(int channel, int id, int value);
    bool setPair(int channel, int id, int first, int second);
    bool saveChannel(int channel);
    std::optional<quint16> keycode(int layer, int row, int col);
    bool setKeycode(int layer, int row, int col, quint16 code);
    std::optional<quint16> encoderKeycode(int layer, int encoder, bool clockwise);
    bool setEncoderKeycode(int layer, int encoder, bool clockwise, quint16 code);
    // Returns layers * rows * cols keycodes, or an empty list on failure.
    QList<quint16> readKeymap();

    void rescan();

signals:
    void connectionChanged(bool connected);

private:
    Definition definition_;
    int fd_ = -1;
    QString node_;
    int protocol_ = 0, layers_ = 0;
    QTimer watch_;

    QByteArray exchange(const QByteArray &request);
    QString findNode() const;
    void closeDevice();
    void check();
};

}
