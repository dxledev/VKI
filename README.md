# VKI

VKI stands for **Virtual Keyboard Interface**.

A small, portable configurator for the **Epomaker Galaxy100 Lite** keyboard. VKI is a
native C++20 / Qt Widgets app with a resizable window, flat colors, and no web runtime.
Set the backlight, remap keys on every layer, and recolor the app from the command line.

## Features

- **Keymap**: view and remap every layer with a keycode picker. The first time a
  keyboard is read, a backup is saved as `keymap-backup.json` in the config directory.
- **Lighting**: brightness, effect, speed and color, read from and written to the keyboard.
- **Themeable**: the whole UI follows a JSON palette that reloads live.
- Dialogs open inside the window, over a soft blur of the content.

The keyboard must be connected with the USB cable (wired mode). VKI talks to the
standard VIA raw HID interface (usage page `0xFF60`).

## Build and run

Requires CMake 3.21+, a C++20 compiler and Qt 6.7+ (Core, Gui, Widgets, OpenGLWidgets;
Test for the tests). On Linux install Qt's Wayland platform plugin for native Wayland.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/vki
```

Install for your own account, including the launcher entry:

```bash
bash scripts/install-desktop.sh --dry-run
bash scripts/install-desktop.sh
```

### Device permissions

Your user needs read and write access to the keyboard's hidraw node. Install the
supplied rule, then replug the keyboard:

```bash
sudo install -Dm644 packaging/60-vki.rules /etc/udev/rules.d/60-vki.rules
sudo udevadm control --reload && sudo udevadm trigger
```

## Your colors

The palette lives in `$XDG_CONFIG_HOME/vki/theme.json` (usually
`~/.config/vki/theme.json`). Every CLI change is written atomically and an open
window reloads right away. Invalid input keeps the last good palette.

```bash
vki theme show
vki theme set 'accent=#a9c7b0' 'surface=#253238'
vki theme set 'background=#e00e1e36'
vki theme set 'background=Qt.rgba(14 / 255, 30 / 255, 54 / 255, 0.88)'
vki theme import /absolute/path/palette.json
vki theme follow file /absolute/path/palette.json
vki theme follow symlink /absolute/path/to/link
vki theme sync
vki theme preset lofi --dry-run
```

`import` takes a snapshot; `follow` keeps watching the source and updates when it
changes. JSON, QML color files, `Qt.rgba(...)` values and terminal palettes work as sources.
Add `--dry-run` to preview any change without writing.

Tokens: `background`, `surface`, `surface_alt`, `text`, `muted`, `accent`, `accent_text`,
`border`, `danger`, `success`, `keycap`, `keycap_text`, `selection`.

Eight-digit colors use Qt's alpha-first `#AARRGGBB`, so `#e00e1e36` is the color
`#0e1e36` at 88% opacity. Translucent backgrounds show through the window when your
compositor supports it; otherwise only the alpha is dropped and the RGB channels stay put.

The app icon is a fixed, multicolored keyboard and is not themed.

## Other options

`--config-dir DIRECTORY` (or `VKI_CONFIG_DIR`) selects a separate configuration.
`--definition FILE` loads a different VIA definition JSON.
