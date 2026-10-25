#!/usr/bin/env bash
set -euo pipefail
out=${1:-build/release-tools}
mkdir -p "$out/runtime-sources/type2-runtime"
fetch() {
    local destination=$1 checksum=$2 url=$3
    if ! test -f "$destination"; then
        curl -fL --retry 3 --retry-all-errors --connect-timeout 15 --max-time 240 \
            -H 'Accept: application/vnd.github.raw+json' -o "$destination" "$url"
    fi
    printf '%s  %s\n' "$checksum" "$destination" | sha256sum -c -
}
fetch "$out/linuxdeploy-x86_64.AppImage" \
    8aea8da0f7f7039d2a2cecb14657d752a222a5e1d3825caeef186c82f751cdd1 \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
fetch "$out/linuxdeploy-plugin-qt-x86_64.AppImage" \
    cfc1055b2b9dbc08412b579f20990b7b41a17b61beaa5847dc9477c96c9e9617 \
    https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
fetch "$out/runtime-x86_64" \
    617aa77314d57c8b8e4fae9c3f0b35af7b30d5529cb69e98f64f92510ed98a46 \
    https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64
fetch "$out/runtime-sources/type2-runtime-fedfad51.tar.gz" \
    a1601c596b81768626afca9abecf0137092d2d7b281bb6c4b9067d2c79e44a21 \
    https://api.github.com/repos/AppImage/type2-runtime/tarball/fedfad51acc2860b2916a1eef32ba4a500ff31ef
fetch "$out/runtime-sources/fuse-3.15.0.tar.xz" \
    70589cfd5e1cff7ccd6ac91c86c01be340b227285c5e200baa284e401eea2ca0 \
    https://github.com/libfuse/libfuse/releases/download/fuse-3.15.0/fuse-3.15.0.tar.xz
fetch "$out/runtime-sources/squashfuse-0.5.2.tar.gz" \
    db0238c5981dabbd80ee09ae15387f390091668ca060a7bc38047912491443d3 \
    https://github.com/vasi/squashfuse/archive/0.5.2.tar.gz
fetch "$out/runtime-sources/musl-1.2.5.tar.gz" \
    a9a118bbe84d8764da0ea0d28b3ab3fae8477fc7e4085d90102b8596fc7c75e4 \
    https://musl.libc.org/releases/musl-1.2.5.tar.gz
fetch "$out/runtime-sources/zstd.LICENSE" \
    7055266497633c9025b777c78eb7235af13922117480ed5c674677adc381c9d8 \
    'https://api.github.com/repos/facebook/zstd/contents/LICENSE?ref=v1.5.6'
fetch "$out/runtime-sources/mimalloc.LICENSE" \
    19c99805e7a44a34b297a75d1edea9985e300066dfc024d5c99d4236d4573b5d \
    'https://api.github.com/repos/microsoft/mimalloc/contents/LICENSE?ref=v2.1.7'
fetch "$out/runtime-sources/zlib.LICENSE" \
    e32ff4e00d9d94930537635291da39e7e612703334bf6fde8c7f1686fe8a45a2 \
    'https://api.github.com/repos/madler/zlib/contents/LICENSE?ref=v1.3.2'
tar -xzf "$out/runtime-sources/type2-runtime-fedfad51.tar.gz" \
    --strip-components=1 -C "$out/runtime-sources/type2-runtime"
cp "$out/runtime-sources/type2-runtime/LICENSE" "$out/runtime-sources/LICENSE"
chmod +x "$out/linuxdeploy-x86_64.AppImage" "$out/linuxdeploy-plugin-qt-x86_64.AppImage" "$out/runtime-x86_64"
