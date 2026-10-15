#!/usr/bin/env bash
# Both entry points open the same native launcher; it runs the game (a Windows program) through Wine, or Proton
# when BLACK_PROTON is set (see docs/linux.md).
set -euo pipefail
release_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)
fail() {
    printf 'BLACK: %s\n' "$*" >&2
    if command -v zenity >/dev/null 2>&1; then
        zenity --error --title='BLACK launcher' --text="$*" 2>/dev/null || true
    fi
    exit 1
}
launcher="$release_root/BLACK Launcher"
[[ -x "$launcher" ]] || fail 'The launcher is missing. Run Build BLACK.sh, or extract the entire release folder.'
[[ $(uname -m) == x86_64 ]] || fail 'This release requires an x86_64 Linux system with an AVX2 CPU.'
cd -- "$release_root"
case "${1:-play}" in
    play) exec "$launcher" --play ;;
    settings) exec "$launcher" ;;
    *) fail 'Expected play or settings.' ;;
esac
