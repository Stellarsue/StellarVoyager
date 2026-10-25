# Linux release build

Run from the repository root on Linux x86_64 with Docker, curl and SHA-256 tools.
Do not reuse an AppDir from a different SDK; use a fresh output directory. The
AppImage requires glibc 2.35+, OpenGL 3.3 and X11/XWayland on the target desktop.

```bash
packaging/fetch-tools.sh build/release-tools
docker build -t stellarvoyager-builder:ubuntu22.04 packaging
mkdir -p build/release-linux
docker run --rm --init --user "$(id -u):$(id -g)" \
  -v "$PWD:/src:ro" -v "$PWD/build/release-linux:/out" \
  -v "$PWD/build/release-tools:/tools:ro" \
  stellarvoyager-builder:ubuntu22.04 /src/packaging/build-linux.sh
```

Outputs are under `build/release-linux/artifacts/`: AppImage, `.deb`, and an archive
of corresponding third-party runtime sources. Tests run before packaging. The
runtime collector records exact Ubuntu source versions, copies license notices,
and fails on unattributed ELF files or unavailable sources. System Qt is dynamically
linked; the AppImage bundles Qt and its plugins, but not glibc or graphics drivers.

Tool downloads are SHA-256 pinned. Upstream `continuous` assets can change: if a
checksum fails, **do not bypass it**. Review the new tool and runtime sources and
update hashes together. Ubuntu repositories are also mutable; the release source
archive and manifests preserve the versions actually shipped, but this is not a
bit-for-bit reproducible build promise. The runtime source archive includes the
AppImage runtime's build scripts, libfuse (with patches in the runtime repository),
squashfuse and musl sources; permissively licensed ancillary runtime components
include their copyright notices.

## Clean-system verification

```bash
for version in 22.04 24.04; do
  docker build -f packaging/Dockerfile.test --build-arg UBUNTU_VERSION="$version" \
    -t "stellarvoyager-test:ubuntu$version" packaging
  docker run --rm --init -v "$PWD/packaging:/packaging:ro" \
    -v "$PWD/build/release-linux:/release:ro" \
    "stellarvoyager-test:ubuntu$version" bash /packaging/test-linux.sh
done
```

These disposable containers first start the AppImage with no system Qt, then
install the `.deb` with its dependencies and start that executable. `--quit-after`
checks the OpenGL context and spacecraft resources before reporting success.
Xvfb uses Mesa software rendering; manually verify on a composited desktop too.
`--init` is necessary for reliable Xvfb startup signals in these containers.

The font test uses temporary user directories; it does not install fonts into the
developer's account. Installed proprietary fonts are never copied into an AppImage.
Only the intact original installer (with EULA) is embedded, and accepting that EULA
is still required before installing its contents.
