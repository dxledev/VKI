#pragma once
#include "core/theme.h"
#include "device/definition.h"
#include "device/via.h"

#include <QString>

namespace vki {

// Persistent "match app accent" switch: a marker file in the config directory.
bool accentSyncEnabled(const QString &directory);
void setAccentSyncEnabled(const QString &directory, bool enabled);
// Writes the theme's accent to the board's colour control and saves it. False when no board answers.
bool applyAccent(ViaDevice &device, const Definition &definition, const Theme &theme);
// Installs/removes and starts/stops the user service that applies the accent while the app is closed.
void setAccentService(const QString &directory, bool enabled);
// Runs headless until stopped: follows the theme and re-applies the accent when the board (re)connects.
int runAccentDaemon(const QString &directory, const QString &definitionPath);

}
