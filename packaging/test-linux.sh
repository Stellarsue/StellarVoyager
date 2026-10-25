#!/usr/bin/env bash
set -euo pipefail
release=${1:-/release}
image="$release/artifacts/StellarVoyager-1.0.0-x86_64.AppImage"
deb="$release/artifacts/StellarVoyager-1.0.0-linux-amd64.deb"
# Run in Dockerfile.test's clean image, with --init for Xvfb signal handling.
if dpkg-query -W libqt6core6 2>/dev/null; then
    echo 'Expected a clean environment without system Qt' >&2
    exit 1
fi
xvfb-run -a -s '-screen 0 1280x900x24 +extension GLX' \
    timeout 45 "$image" --appimage-extract-and-run --no-font-prompt --quit-after 5
apt-get install -y --no-install-recommends "$deb"
test -f /usr/share/applications/linux-system-monitor.desktop
xvfb-run -a -s '-screen 0 1280x900x24 +extension GLX' \
    timeout 45 /usr/bin/system-monitor --no-font-prompt --quit-after 5
echo 'PASS: portable AppImage without system Qt, native DEB installation and OpenGL startup'
