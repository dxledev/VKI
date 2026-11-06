#include "accent_sync.h"

#include "core/storage.h"
#include "ui/theme_watcher.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <algorithm>
#include <QCryptographicHash>
#include <QSysInfo>
#include <signal.h>
#include <unistd.h>

namespace vki {
namespace {

QString flagPath(const QString &directory) { return QDir(directory).filePath("accent-sync"); }

// One unit per configuration directory, so separate configs never touch each other's service.
QString serviceName(const QString &directory) {
    const auto hash = QCryptographicHash::hash(QDir::cleanPath(directory).toUtf8(), QCryptographicHash::Sha1).toHex();
    return QString("vki-accent-%1.service").arg(QString::fromLatin1(hash.left(8)));
}

QString unitPath(const QString &directory) {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
        .filePath(QString("systemd/user/%1").arg(serviceName(directory)));
}

QString quoted(const QString &value) {
    QString out = value;
    out.replace('\\', "\\\\").replace('"', "\\\"").replace('%', "%%").replace('$', "$$").remove('\n');
    return '"' + out + '"';
}

// True while a VKI window of this configuration is running (stale locks from crashes don't count).
bool windowOpen(const QString &directory) {
    qint64 pid = 0;
    QString host, app;
    QLockFile lock(QDir(directory).filePath("session.lock"));
    if (!lock.getLockInfo(&pid, &host, &app) || pid <= 0 || pid == ::getpid()) return false;
    if (!host.isEmpty() && host != QSysInfo::machineHostName()) return false;
    QFile comm(QString("/proc/%1/comm").arg(pid));
    return comm.open(QIODevice::ReadOnly) && comm.readAll().trimmed() == "vki";
}

QString accentKey(const Theme &theme) { return theme.color("accent").name(); }

}

bool accentSyncEnabled(const QString &directory) { return QFileInfo::exists(flagPath(directory)); }

void setAccentSyncEnabled(const QString &directory, bool enabled) {
    if (enabled) {
        QDir().mkpath(directory);
        QSaveFile file(flagPath(directory));
        if (file.open(QIODevice::WriteOnly)) { file.write("on\n"); file.commit(); }
    } else {
        QFile::remove(flagPath(directory));
    }
}

bool applyAccent(ViaDevice &device, const Definition &definition, const Theme &theme) {
    const QColor c = theme.color("accent");
    const int hue = std::clamp(c.hsvHue() < 0 ? 0 : c.hsvHue() * 256 / 360, 0, 255);
    for (const auto &section : definition.sections)
        for (const auto &control : section.controls)
            if (control.type == Control::Color) {
                const auto current = device.getPair(control.channel, control.id);
                if (current && current->first == hue && current->second == c.hsvSaturation()) return true;
                return device.setPair(control.channel, control.id, hue, c.hsvSaturation()) &&
                       device.saveChannel(control.channel);
            }
    return false;
}

void setAccentService(const QString &directory, bool enabled) {
    const auto name = serviceName(directory);
    const auto path = unitPath(directory);
    auto systemctl = [](const QString &command) {
        QProcess::startDetached("sh", {"-c", "systemctl --user " + command + " >/dev/null 2>&1"});
    };
    if (!enabled) {
        if (!QFileInfo::exists(path)) return;
        QProcess::startDetached("sh", {"-c", QString("systemctl --user disable --now %1 >/dev/null 2>&1; rm -f '%2'; "
                                                      "systemctl --user daemon-reload >/dev/null 2>&1")
                                                 .arg(name, path)});
        return;
    }
    QString binary = qEnvironmentVariable("APPIMAGE");
    if (binary.isEmpty()) binary = QCoreApplication::applicationFilePath();
    QString arguments = "--config-dir " + quoted(directory);
    const auto args = QCoreApplication::arguments();
    for (qsizetype i = 0; i + 1 < args.size(); ++i)
        if (args[i] == "--definition") arguments += " --definition " + quoted(QFileInfo(args[i + 1]).absoluteFilePath());
    const QByteArray unit = QString("[Unit]\nDescription=VKI keyboard accent sync\n"
                                    "ConditionPathExists=%1\nConditionPathExists=%2\n"
                                    "StartLimitIntervalSec=60\nStartLimitBurst=5\n\n[Service]\n"
                                    "ExecStart=%3 %4 accent-daemon\nRestart=on-failure\nRestartSec=5\n\n"
                                    "[Install]\nWantedBy=default.target\n")
                                .arg(binary, flagPath(directory), quoted(binary), arguments)
                                .toUtf8();
    QFile existing(path);
    const bool same = existing.open(QIODevice::ReadOnly) && existing.readAll() == unit;
    existing.close();
    if (!same) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || (file.write(unit), !file.commit())) return;
        systemctl("daemon-reload");
    }
    systemctl("enable --now " + name);
    if (!same) systemctl("restart " + name);
}

int runAccentDaemon(const QString &directory, const QString &definitionPath) {
    const auto definition = loadDefinition(definitionPath);
    QDir().mkpath(directory);
    ThemeWatcher watcher(directory + "/theme.json", ensureTheme(directory + "/theme.json"));
    QString applied;  // accent last written to the board (it is saved there, so replugging keeps it)
    // The board is only opened for the moment of a write, and never while a window owns it.
    auto sync = [&] {
        if (!accentSyncEnabled(directory)) { QCoreApplication::exit(0); return; }
        if (windowOpen(directory)) return;
        const auto key = accentKey(watcher.theme());
        if (key == applied) return;
        ViaDevice device(definition);
        if (device.connected() && applyAccent(device, definition, watcher.theme())) applied = key;
    };
    QObject::connect(&watcher, &ThemeWatcher::changed, [&] { sync(); });
    QTimer retry;  // retries when the board was missing or a window was open, and notices the switch going off
    retry.setInterval(3000);
    QObject::connect(&retry, &QTimer::timeout, [&] { sync(); });
    retry.start();
    QTimer::singleShot(0, [&] { sync(); });
    return QCoreApplication::exec();
}

}
