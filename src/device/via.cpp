#include "via.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

namespace vki {
namespace {

constexpr int reportSize = 32;

bool descriptorIsVia(const QByteArray &descriptor) {
    // Usage page 0xFF60 (06 60 FF) followed by usage 0x61 (09 61).
    return descriptor.contains(QByteArray::fromHex("0660ff0961"));
}

QString ueventValue(const QString &path, const QString &key) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    for (const auto &line : file.readAll().split('\n'))
        if (line.startsWith(key.toUtf8() + "="))
            return QString::fromUtf8(line.mid(key.size() + 1));
    return {};
}

}

ViaDevice::ViaDevice(const Definition &definition, QObject *parent)
    : QObject(parent), definition_(definition) {
    watch_.setInterval(1500);
    connect(&watch_, &QTimer::timeout, this, &ViaDevice::check);
    watch_.start();
    rescan();
}

ViaDevice::~ViaDevice() {
    closeDevice();
}

QString ViaDevice::findNode() const {
    const QString wanted = QString("0003:%1:%2")
                               .arg(definition_.vendorId, 8, 16, QChar('0'))
                               .arg(definition_.productId, 8, 16, QChar('0'))
                               .toUpper();
    const QDir sys("/sys/class/hidraw");
    for (const auto &name : sys.entryList({"hidraw*"}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString device = sys.filePath(name) + "/device";
        if (ueventValue(device + "/uevent", "HID_ID").toUpper() != wanted)
            continue;
        QFile descriptor(device + "/report_descriptor");
        if (descriptor.open(QIODevice::ReadOnly) && descriptorIsVia(descriptor.readAll()))
            return "/dev/" + name;
    }
    return {};
}

void ViaDevice::rescan() {
    if (fd_ >= 0)
        return;
    const auto node = findNode();
    if (node.isEmpty())
        return;
    const int fd = ::open(node.toLocal8Bit().constData(), O_RDWR | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0)
        return;
    fd_ = fd;
    node_ = node;
    const auto version = exchange(QByteArray(1, char(0x01)));
    if (version.size() < 3) {
        closeDevice();
        return;
    }
    protocol_ = (uchar(version[1]) << 8) | uchar(version[2]);
    const auto layers = exchange(QByteArray(1, char(0x11)));
    layers_ = layers.size() >= 2 ? std::clamp(int(uchar(layers[1])), 1, 16) : 1;
    emit connectionChanged(true);
}

void ViaDevice::closeDevice() {
    if (fd_ >= 0)
        ::close(fd_);
    fd_ = -1;
    node_.clear();
}

void ViaDevice::check() {
    if (fd_ < 0) {
        rescan();
        return;
    }
    if (!QFileInfo::exists(node_)) {
        closeDevice();
        emit connectionChanged(false);
    }
}

QByteArray ViaDevice::exchange(const QByteArray &request) {
    if (fd_ < 0)
        return {};
    QByteArray packet(reportSize + 1, '\0');  // leading report id 0
    for (qsizetype i = 0; i < request.size() && i < reportSize; ++i)
        packet[i + 1] = request[i];
    // Drain stale input so the next report belongs to this request.
    char scratch[64];
    while (::read(fd_, scratch, sizeof scratch) > 0) {}
    ssize_t written;
    do {
        written = ::write(fd_, packet.constData(), packet.size());
    } while (written < 0 && errno == EINTR);
    if (written != packet.size()) {
        if (errno == ENODEV || errno == EIO) {
            closeDevice();
            emit connectionChanged(false);
        }
        return {};
    }
    pollfd waiter{fd_, POLLIN, 0};
    if (::poll(&waiter, 1, 400) <= 0)
        return {};
    QByteArray response(64, '\0');
    const auto count = ::read(fd_, response.data(), response.size());
    if (count < 1)
        return {};
    response.resize(count);
    // The first byte echoes the command; mismatches mean the board rejected it.
    if (response.isEmpty() || uchar(response[0]) != uchar(request[0]))
        return {};
    return response;
}

std::optional<int> ViaDevice::getValue(int channel, int id) {
    const auto reply = exchange(QByteArray::fromHex("08") + QByteArray(1, char(channel)) + QByteArray(1, char(id)));
    if (reply.size() < 4)
        return std::nullopt;
    return uchar(reply[3]);
}

std::optional<QPair<int, int>> ViaDevice::getPair(int channel, int id) {
    const auto reply = exchange(QByteArray::fromHex("08") + QByteArray(1, char(channel)) + QByteArray(1, char(id)));
    if (reply.size() < 5)
        return std::nullopt;
    return QPair<int, int>{uchar(reply[3]), uchar(reply[4])};
}

bool ViaDevice::setValue(int channel, int id, int value) {
    const auto request = QByteArray::fromHex("07") + QByteArray(1, char(channel)) + QByteArray(1, char(id))
                         + QByteArray(1, char(value));
    return exchange(request).size() >= 4;
}

bool ViaDevice::setPair(int channel, int id, int first, int second) {
    const auto request = QByteArray::fromHex("07") + QByteArray(1, char(channel)) + QByteArray(1, char(id))
                         + QByteArray(1, char(first)) + QByteArray(1, char(second));
    return exchange(request).size() >= 5;
}

bool ViaDevice::saveChannel(int channel) {
    return !exchange(QByteArray::fromHex("09") + QByteArray(1, char(channel))).isEmpty();
}

std::optional<quint16> ViaDevice::keycode(int layer, int row, int col) {
    const auto reply = exchange(QByteArray::fromHex("04") + QByteArray(1, char(layer)) + QByteArray(1, char(row))
                                + QByteArray(1, char(col)));
    if (reply.size() < 6)
        return std::nullopt;
    return quint16((uchar(reply[4]) << 8) | uchar(reply[5]));
}

bool ViaDevice::setKeycode(int layer, int row, int col, quint16 code) {
    QByteArray request = QByteArray::fromHex("05");
    request.append(char(layer)).append(char(row)).append(char(col)).append(char(code >> 8)).append(char(code & 0xff));
    return exchange(request).size() >= 6;
}

std::optional<quint16> ViaDevice::encoderKeycode(int layer, int encoder, bool clockwise) {
    const auto reply = exchange(QByteArray::fromHex("14") + QByteArray(1, char(layer)) + QByteArray(1, char(encoder))
                                + QByteArray(1, char(clockwise ? 1 : 0)));
    if (reply.size() < 6)
        return std::nullopt;
    return quint16((uchar(reply[4]) << 8) | uchar(reply[5]));
}

bool ViaDevice::setEncoderKeycode(int layer, int encoder, bool clockwise, quint16 code) {
    QByteArray request = QByteArray::fromHex("15");
    request.append(char(layer)).append(char(encoder)).append(char(clockwise ? 1 : 0))
        .append(char(code >> 8)).append(char(code & 0xff));
    return exchange(request).size() >= 6;
}

QList<quint16> ViaDevice::readKeymap() {
    const int total = layers_ * definition_.rows * definition_.cols * 2;
    QByteArray buffer;
    while (buffer.size() < total) {
        const int size = std::min(28, total - int(buffer.size()));
        QByteArray request = QByteArray::fromHex("12");
        request.append(char(buffer.size() >> 8)).append(char(buffer.size() & 0xff)).append(char(size));
        const auto reply = exchange(request);
        if (reply.size() < 4 + size)
            return {};
        buffer.append(reply.mid(4, size));
    }
    QList<quint16> result;
    for (qsizetype i = 0; i + 1 < buffer.size(); i += 2)
        result.append(quint16((uchar(buffer[i]) << 8) | uchar(buffer[i + 1])));
    return result;
}

}
