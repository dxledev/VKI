#pragma once
#include <QColor>
#include <QIcon>
#include <QString>

namespace vki {
// Monochrome glyphs drawn as vector paths, tinted with a theme color.
QIcon glyph(const QString &name, const QColor &color, int size = 22);
}
