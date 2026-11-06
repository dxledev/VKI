#!/usr/bin/env bash
set -euo pipefail
project_directory="$(cd -- "${BASH_SOURCE[0]%/*}/.." && pwd)"
build_directory="$project_directory/build"
install_prefix="$HOME/.local"
config_directory="${XDG_CONFIG_HOME:-$HOME/.config}/vki"
apps_list="${XDG_CONFIG_HOME:-$HOME/.config}/apps.list"
follow_path=""
dry_run=0
usage() {
    printf '%s\n' 'Usage: install-desktop.sh [--dry-run] [--build-dir DIR] [--prefix DIR]' \
        '       [--config-dir DIR] [--apps-list FILE] [--follow PALETTE_DIRECTORY]'
}
fail() { printf 'install-desktop: %s\n' "$*" >&2; exit 1; }
parse_options() {
    while (( $# )); do
        case "$1" in
            --dry-run) dry_run=1; shift ;;
            --help|-h) usage; exit 0 ;;
            --build-dir|--prefix|--config-dir|--apps-list|--follow)
                (( $# >= 2 )) || fail "Missing value for $1"
                [[ "$2" == /* && "$2" != / && "$2" != *$'\n'* && "$2" != *'|'* && "$2" != *'"'* && "$2" != *'%'* && "$2" != *'\\'* && "$2" != *'`'* && "$2" != *'$'* ]] || fail "Expected a safe absolute path for $1"
                case "$1" in
                    --build-dir) build_directory="${2%/}" ;;
                    --prefix) install_prefix="${2%/}" ;;
                    --config-dir) config_directory="${2%/}" ;;
                    --apps-list) apps_list="$2" ;;
                    --follow) follow_path="${2%/}" ;;
                esac
                shift 2 ;;
            *) fail "Unknown option: $1" ;;
        esac
    done
}
validate() {
    [[ -x "$build_directory/vki" ]] || fail "Build first: $build_directory/vki"
    [[ ! -L "$apps_list" || -f "$apps_list" ]] || fail 'Invalid app list'
    [[ ! -L "$install_prefix/bin/vki" ]] || fail 'Executable path is a symlink'
    local output
    for output in "$install_prefix/bin/applications/io.github.vki.desktop" "$install_prefix/share/applications/io.github.vki.desktop" "$install_prefix/share/icons/hicolor/scalable/apps/io.github.vki.svg"; do
        [[ ! -L "$output" && ( ! -e "$output" || -f "$output" ) ]] || fail "Refusing to replace an unexpected output: $output"
    done
    if [[ -n "$follow_path" ]]; then
        "$build_directory/vki" --config-dir "$config_directory" theme follow desktop "$follow_path" --dry-run >/dev/null
    fi
}
install_files() {
    local temporary desktop="$install_prefix/bin/applications/io.github.vki.desktop"
    /usr/bin/mkdir -p -- "$install_prefix/bin" "${desktop%/*}" "$install_prefix/share/applications"
    temporary="$(/usr/bin/mktemp "$install_prefix/bin/.vki.XXXXXX")"
    /usr/bin/install -m755 -- "$build_directory/vki" "$temporary"
    /usr/bin/mv -T -- "$temporary" "$install_prefix/bin/vki"
    /usr/bin/install -Dm644 -- "$project_directory/assets/io.github.vki.svg" "$install_prefix/share/icons/hicolor/scalable/apps/io.github.vki.svg"
    /usr/bin/awk -v executable="$install_prefix/bin/vki" -v config="$config_directory" \
        '/^Exec=/ {print "Exec=\"" executable "\" --config-dir \"" config "\"";next} {print}' "$project_directory/assets/io.github.vki.desktop" > "$desktop"
    /usr/bin/install -m644 -- "$desktop" "$install_prefix/share/applications/io.github.vki.desktop"
    if [[ -x /usr/bin/update-desktop-database ]]; then /usr/bin/update-desktop-database "$install_prefix/share/applications"; fi
}
register_app() {
    local target="$apps_list" temporary
    if [[ -L "$target" ]]; then target="$(/usr/bin/readlink -f -- "$target")"; fi
    /usr/bin/mkdir -p -- "${target%/*}"
    if [[ -f "$target" ]] && /usr/bin/awk -F '|' '$1=="io.github.vki.desktop" {found=1} END {exit !found}' "$target"; then return; fi
    temporary="$(/usr/bin/mktemp "${target%/*}/.vki-apps.XXXXXX")"
    if [[ -f "$target" ]]; then /usr/bin/cp -p -- "$target" "$target.vki-backup.$(/usr/bin/date +%Y%m%dT%H%M%S).$$"; /usr/bin/awk '{print}' "$target" > "$temporary"; /usr/bin/chmod --reference="$target" "$temporary"; fi
    printf 'io.github.vki.desktop|%s/share/icons/hicolor/scalable/apps/io.github.vki.svg|VKI\n' "$install_prefix" >> "$temporary"
    /usr/bin/mv -T -- "$temporary" "$target"
}
main() {
    parse_options "$@";validate
    if (( dry_run )); then
        printf 'Install: %s/bin/vki\nDesktop: %s/bin/applications/io.github.vki.desktop\nApp list: %s\nPalette: %s\n' "$install_prefix" "$install_prefix" "$apps_list" "${follow_path:-saved palette or default}"
        return
    fi
    install_files;register_app
    if [[ -n "$follow_path" ]]; then "$install_prefix/bin/vki" --config-dir "$config_directory" theme follow desktop "$follow_path" >/dev/null; fi
    printf 'Installed VKI.\n'
}
main "$@"
