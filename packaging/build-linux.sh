#!/usr/bin/env bash
set -euo pipefail
src=${1:-/src}
out=${2:-/out}
test -f "$src/CMakeLists.txt"
test "$out" != / && test "$out" != "$src"
mkdir -p "$out/cmake" "$out/artifacts"
cmake -S "$src" -B "$out/cmake" -DCMAKE_BUILD_TYPE=Release \
    -DSTELLARSUE_BUILD_SMOKE_TESTS=ON -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "$out/cmake" -j2
QT_QPA_PLATFORM=offscreen "$out/cmake/stellarsue-font-test"
xvfb-run -a -s '-screen 0 1280x900x24 +extension GLX' \
    "$out/cmake/stellarsue-smoke" "$out/previews"
DESTDIR="$out/AppDir" cmake --install "$out/cmake"
desktop-file-validate "$out/AppDir/usr/share/applications/linux-system-monitor.desktop"
(cd "$out/cmake" && cpack -G DEB -B "$out/artifacts")

mkdir -p "$out/AppDir/usr/share/fonts/stellarsue"
cp /usr/share/fonts/truetype/liberation2/LiberationSerif-*.ttf \
    /usr/share/fonts/opentype/noto/NotoSerifCJK-Regular.ttc \
    "$out/AppDir/usr/share/fonts/stellarsue/"
export APPIMAGE_EXTRACT_AND_RUN=1 QMAKE=/usr/bin/qmake6
export PATH="/tools:$PATH" EXTRA_QT_PLUGINS=iconengines
/tools/linuxdeploy-x86_64.AppImage --appdir "$out/AppDir" \
    --executable /usr/bin/cabextract --executable /usr/bin/fc-cache \
    --desktop-file "$src/linux-system-monitor.desktop" \
    --icon-file "$src/icons/linux-system-monitor.svg" \
    --custom-apprun "$src/packaging/AppRun" --plugin qt
python3 "$src/packaging/collect-runtime.py" "$out/AppDir" "$out/runtime-sources"
XZ_OPT='-T2 -1' tar -cJf "$out/artifacts/StellarVoyager-1.0.0-runtime-sources.tar.xz" \
    -C "$out" runtime-sources
export VERSION=1.0.0 OUTPUT="$out/artifacts/StellarVoyager-1.0.0-x86_64.AppImage"
export LDAI_RUNTIME_FILE=/tools/runtime-x86_64
/tools/linuxdeploy-x86_64.AppImage --appdir "$out/AppDir" --output appimage
