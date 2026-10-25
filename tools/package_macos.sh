#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != Darwin ]]; then
    printf 'This packaging script requires macOS.\n' >&2
    exit 1
fi

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
executable="${1:-${project_root}/build/system-monitor}"
bundle="${2:-${project_root}/build/StellarVoyager.app}"
icon_source="${project_root}/icons/stellarvoyager-logo.png"

if [[ ! -x "${executable}" || ! -f "${icon_source}" ]]; then
    printf 'Missing executable or app icon. Build the project first.\n' >&2
    exit 1
fi
if [[ -e "${bundle}" ]]; then
    printf 'Bundle already exists; choose a new output path: %s\n' "${bundle}" >&2
    exit 1
fi
if ! command -v macdeployqt >/dev/null 2>&1; then
    printf 'macdeployqt is required to bundle the Qt runtime.\n' >&2
    exit 1
fi

mkdir -p "${bundle}/Contents/MacOS" "${bundle}/Contents/Resources"
cp "${executable}" "${bundle}/Contents/MacOS/system-monitor"
cp "${project_root}/macos/Info.plist" "${bundle}/Contents/Info.plist"

icon_work="$(mktemp -d "${TMPDIR:-/tmp}/stellarvoyager-icon.XXXXXXXX")"
trap 'rm -r "${icon_work}"' EXIT
iconset="${icon_work}/StellarVoyager.iconset"
mkdir "${iconset}"
for size in 16 32 128 256 512; do
    sips -s format png -z "${size}" "${size}" "${icon_source}" \
        --out "${iconset}/icon_${size}x${size}.png" >/dev/null
    double_size="$((size * 2))"
    sips -s format png -z "${double_size}" "${double_size}" "${icon_source}" \
        --out "${iconset}/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil -c icns "${iconset}" -o "${bundle}/Contents/Resources/StellarVoyager.icns"

macdeployqt "${bundle}" -always-overwrite
if [[ ! -f "${bundle}/Contents/PlugIns/platforms/libqcocoa.dylib" ]] \
    || otool -L "${bundle}/Contents/MacOS/system-monitor" | grep -q '/usr/local/opt/qt/'; then
    printf 'Qt deployment is incomplete; the app bundle was not installed.\n' >&2
    exit 1
fi
codesign --force --deep --sign - "${bundle}"
plutil -lint "${bundle}/Contents/Info.plist"
codesign --verify --deep --strict "${bundle}"
printf 'Packaged %s\n' "${bundle}"
