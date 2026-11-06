#!/usr/bin/env bash
set -euo pipefail
bin="$1"; dir="$(mktemp -d)"; trap 'rm -rf "$dir"' EXIT
"$bin" --config-dir "$dir" theme set 'accent=#112233' 'background=#e00e1e36' >/dev/null
grep -q '"accent": "#112233"' "$dir/theme.json"
grep -q '"background": "#e00e1e36"' "$dir/theme.json"
"$bin" --config-dir "$dir" theme set 'background=Qt.rgba(14 / 255, 30 / 255, 54 / 255, 0.88)' >/dev/null
"$bin" --config-dir "$dir" theme show | grep -q '"background"'
before="$(cat "$dir/theme.json")"
"$bin" --config-dir "$dir" theme set 'accent=nonsense' 2>/dev/null && exit 1
[[ "$before" == "$(cat "$dir/theme.json")" ]]
echo ok
