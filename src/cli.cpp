#include "cli.h"

#include "core/storage.h"
#include "core/theme.h"
#include "core/theme_source.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTextStream>
#include <stdexcept>

namespace vki {
namespace {

[[noreturn]] void fail(const QString &message) {
    throw std::invalid_argument(message.toStdString());
}

void requireCount(const QStringList &arguments, qsizetype count) {
    if (arguments.size() != count)
        fail("Invalid arguments. Run vki --help for usage.");
}

void printTheme(const Theme &theme) {
    QTextStream(stdout) << QJsonDocument(theme.toJson()).toJson(QJsonDocument::Indented);
}

Theme currentTheme(const QString &path) {
    return QFileInfo::exists(path) ? loadTheme(path) : presetTheme("lofi");
}

Theme proposedTheme(const QString &path, const QString &command, const QStringList &arguments) {
    if (command == "preset") {
        requireCount(arguments, 1);
        return presetTheme(arguments.first());
    }
    if (command == "set")
        return setColors(currentTheme(path), arguments);
    if (command == "import") {
        requireCount(arguments, 1);
        return importPalette(QFileInfo(arguments.first()).absoluteFilePath());
    }
    if (command == "follow") {
        if (arguments.isEmpty() || arguments.size() > 2)
            fail("Expected theme follow file|symlink [PATH]");
        const auto kind = arguments.first();
        if ((kind == "file" || kind == "symlink") && arguments.size() != 2)
            fail("theme follow " + kind + " requires a path");
        auto result = followTheme(kind, arguments.value(1));
        if (sourceTargetsFile(result, path))
            fail("A theme cannot follow its own output file");
        return result;
    }
    if (command == "sync") {
        requireCount(arguments, 0);
        auto result = currentTheme(path);
        if (result.sourceKind.isEmpty())
            fail("The current theme does not follow a source");
        result = refreshSource(result);
        if (sourceTargetsFile(result, path))
            fail("A theme cannot follow its own output file");
        return result;
    }
    fail("Unknown theme command: " + command);
}

}

void printHelp() {
    QTextStream(stdout) << R"(VKI — a standalone configurator for the Epomaker Galaxy100 Lite

Usage:
  vki [--config-dir DIRECTORY] [--definition FILE]
  vki theme show
  vki theme set 'accent=#c7a77a' 'background=#1b1a18' [--dry-run]
  vki theme import FILE [--dry-run]
  vki theme follow file PATH [--dry-run]
  vki theme follow symlink PATH [--dry-run]
  vki theme sync [--dry-run]
  vki theme preset lofi [--dry-run]
  vki --version

Theme changes are saved atomically to theme.json and open windows follow the
saved palette. A followed symlink is checked again when its target is replaced.
JSON, QML, Qt.rgba, and kitty palette files are supported. Eight-digit colors
use alpha-first #AARRGGBB.
--dry-run prints the proposed theme without writing files.
All commands accept --config-dir DIRECTORY or VKI_CONFIG_DIR.
)";
}

int runCli(QStringList arguments) {
    const bool helpRequested = arguments.contains("--help") || arguments.contains("-h");
    const bool versionRequested = arguments.contains("--version");
    const bool hasThemeCommand = arguments.contains("theme");
    if (!helpRequested && !versionRequested && !hasThemeCommand) {
        if (arguments.contains("--dry-run")) {
            QTextStream(stderr) << "vki: --dry-run requires a theme command\n";
            return 2;
        }
        try {
            configDirectory(arguments);
            for (qsizetype index = 0; index < arguments.size(); ++index) {
                if (arguments[index] == "--config-dir" || arguments[index] == "--definition") {
                    ++index;
                    continue;
                }
                fail("Unknown option or argument: " + arguments[index]);
            }
            return -1;
        } catch (const std::exception &error) {
            QTextStream(stderr) << "vki: " << error.what() << '\n';
            return 2;
        }
    }

    try {
        const QString config = configDirectory(arguments);
        if (helpRequested) {
            printHelp();
            return 0;
        }
        if (versionRequested) {
            QTextStream(stdout) << "vki 1.0.0\n";
            return 0;
        }

        bool dryRun = false;
        QStringList commands;
        for (qsizetype index = 0; index < arguments.size(); ++index) {
            const auto &argument = arguments[index];
            if (argument == "--config-dir" || argument == "--definition") {
                ++index;
            } else if (argument == "--dry-run") {
                dryRun = true;
            } else if (argument.startsWith('-')) {
                fail("Unknown option: " + argument);
            } else {
                commands.append(argument);
            }
        }
        if (commands.size() < 2 || commands[0] != "theme")
            fail("Expected a theme command. Run vki --help.");

        const QString outputPath = QDir(config).filePath("theme.json");
        const QString command = commands[1];
        const QStringList commandArguments = commands.mid(2);
        if (command == "show") {
            requireCount(commandArguments, 0);
            printTheme(currentTheme(outputPath));
            return 0;
        }

        if (dryRun) {
            printTheme(proposedTheme(outputPath, command, commandArguments));
            return 0;
        }

        const auto updated = updateThemeLocked(outputPath, [&](const Theme &current) {
            if (command == "preset") {
                requireCount(commandArguments, 1);
                return presetTheme(commandArguments.first());
            }
            if (command == "set")
                return setColors(current, commandArguments);
            if (command == "import") {
                requireCount(commandArguments, 1);
                return importPalette(QFileInfo(commandArguments.first()).absoluteFilePath());
            }
            if (command == "follow") {
                if (commandArguments.isEmpty() || commandArguments.size() > 2)
                    fail("Expected theme follow file|symlink [PATH]");
                const auto kind = commandArguments.first();
                if ((kind == "file" || kind == "symlink") && commandArguments.size() != 2)
                    fail("theme follow " + kind + " requires a path");
                auto result = followTheme(kind, commandArguments.value(1));
                if (sourceTargetsFile(result, outputPath))
                    fail("A theme cannot follow its own output file");
                return result;
            }
            if (command == "sync") {
                requireCount(commandArguments, 0);
                if (current.sourceKind.isEmpty())
                    fail("The current theme does not follow a source");
                auto result = refreshSource(current);
                if (sourceTargetsFile(result, outputPath))
                    fail("A theme cannot follow its own output file");
                return result;
            }
            fail("Unknown theme command: " + command);
        });
        Q_UNUSED(updated);
        QTextStream(stdout) << "Saved " << outputPath << '\n';
        return 0;
    } catch (const std::exception &error) {
        QTextStream(stderr) << "vki: " << error.what() << '\n';
        return 2;
    }
}

}
