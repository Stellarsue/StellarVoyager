# Third-party notices for the Linux release

StellarVoyager's application source is available in the matching tag at
https://github.com/Stellarsue/StellarVoyager. No general license has yet been
selected for the application itself; the components below retain their own terms.

## Qt and bundled runtime libraries

The AppImage dynamically links Qt 6 (Copyright The Qt Company Ltd. and other
contributors), distributed under the applicable LGPL v3 terms. Other bundled
libraries and utilities retain their individual licenses. Exact package versions,
copyright notices and license texts are included under
`usr/share/doc/stellarsue/runtime-licenses/` in the extracted AppImage.

The accompanying `StellarVoyager-1.0.0-runtime-sources.tar.xz` release asset contains
the corresponding Ubuntu source packages, including distribution patches and build
rules, for the bundled runtime components. See its manifest for exact versions.
These are unmodified distribution binaries except for deployment-related RPATH
adjustments and symbol stripping. The `.deb` instead uses system libraries supplied
by the user's distribution.

To inspect or replace dynamically linked libraries, run the AppImage with
`--appimage-extract`, replace compatible libraries/plugins in `squashfs-root/usr/lib`
and `squashfs-root/usr/plugins`, and launch `squashfs-root/AppRun`. No signature check
prevents this. Modification of LGPL-covered libraries and reverse engineering of
the combined application for debugging such modifications are permitted as
required by their LGPL terms. Rebuild the application with CMake if needed.

## Fonts

The AppImage includes unchanged Liberation Serif and Noto Serif CJK fallback fonts
under their SIL Open Font License terms. Their notices are in `runtime-licenses`.
They are loaded privately by the application, not installed globally.

The optional Times New Roman payload is the **intact original** Microsoft core-font
installer, with its original EULA. Installing it requires explicit acceptance and
affects only the current user's font directory. It is not licensed for for-profit
redistribution. No standalone Microsoft TTF files are redistributed by this project.
See `corefonts/EULA.html` and `corefonts/SOURCE.md` next to this document.

## Spacecraft

The NASA Space Shuttle model and its textures are credited in
`nasa-shuttle/SOURCE.md`. Their use does not imply NASA endorsement.

## AppImage runtime

The executable AppImage runtime is built by the AppImage/type2-runtime project.
Its source and license notices accompany the runtime source archive. Packaging tools
are recorded with checksums in the build manifest. Host graphics drivers and glibc
are not bundled; OpenGL 3.3 and a compatible Linux x86_64 desktop are required.
