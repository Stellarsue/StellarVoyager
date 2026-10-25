#!/usr/bin/env python3
"""Collect exact Ubuntu runtime sources and notices; fail on unattributed ELF files."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile


def run(*args):
    return subprocess.check_output(args, text=True).strip()


appdir, sources = (Path(p).resolve() for p in sys.argv[1:])
sources.mkdir(parents=True, exist_ok=True)
notices = appdir / "usr/share/doc/stellarsue/runtime-licenses"
notices.mkdir(parents=True, exist_ok=True)
owners = {}
for listing in Path("/var/lib/dpkg/info").glob("*.list"):
    package = listing.name.removesuffix(".list")
    for name in listing.read_text().splitlines():
        path = Path(name)
        if path.is_file():
            owners.setdefault(path.name, set()).add(package)

packages = {"fonts-liberation2", "fonts-noto-cjk"}
files = []
for path in sorted((appdir / "usr").rglob("*")):
    if not path.is_file() or path.is_symlink():
        continue
    with path.open("rb") as stream:
        is_elf = stream.read(4) == b"\x7fELF"
    if not is_elf or path.name == "system-monitor":
        continue
    candidates = owners.get(path.name, set())
    if len(candidates) != 1:
        raise RuntimeError(f"Cannot uniquely attribute {path}: {candidates}")
    package = next(iter(candidates))
    packages.add(package)
    files.append({"path": str(path.relative_to(appdir)), "package": package,
                  "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})

records = []
source_packages = set()
for package in sorted(packages):
    version, source, source_version = run(
        "dpkg-query", "-W", "-f=${Version}\t${source:Package}\t${source:Version}", package
    ).split("\t")
    copyright_file = Path("/usr/share/doc") / package.split(":")[0] / "copyright"
    if not copyright_file.is_file():
        raise RuntimeError(f"Missing copyright notice for {package}")
    shutil.copyfile(copyright_file, notices / (package + ".copyright"))
    records.append({"binary_package": package, "version": version,
                    "source_package": source, "source_version": source_version})
    # The OFL permits unmodified font redistribution with its notices.
    if not package.startswith("fonts-"):
        source_packages.add((source, source_version))
shutil.copytree("/usr/share/common-licenses", notices / "common-licenses", dirs_exist_ok=True)
manifest = {"distribution": "Ubuntu 22.04 amd64", "packages": records, "files": files,
            "modifications": "linuxdeploy RPATH adjustment and symbol stripping only"}
for destination in (sources / "manifest.json", notices / "manifest.json"):
    destination.write_text(json.dumps(manifest, indent=2) + "\n")
for source, version in sorted(source_packages):
    print(f"Collecting corresponding source: {source}={version}", flush=True)
    subprocess.run(["apt-get", "source", "--download-only", "-y", f"{source}={version}"],
                   cwd=sources, check=True)

# AppImage runtime sources/notices are prepared by fetch-tools.sh, pinned by hash.
runtime_sources = Path("/tools/runtime-sources")
if not runtime_sources.is_dir():
    raise RuntimeError("Missing pinned AppImage runtime source bundle in /tools/runtime-sources")
shutil.copytree(runtime_sources, sources / "appimage-runtime", dirs_exist_ok=True)
shutil.copyfile(runtime_sources / "LICENSE", notices / "AppImage-runtime.LICENSE")
for path in runtime_sources.glob("*.LICENSE"):
    shutil.copyfile(path, notices / path.name)
for archive, member, filename in (
    ("fuse-3.15.0.tar.xz", "fuse-3.15.0/LGPL2.txt", "libfuse.LICENSE"),
    ("squashfuse-0.5.2.tar.gz", "squashfuse-0.5.2/LICENSE", "squashfuse.LICENSE"),
    ("musl-1.2.5.tar.gz", "musl-1.2.5/COPYRIGHT", "musl.LICENSE"),
):
    with tarfile.open(runtime_sources / archive) as tar:
        with tar.extractfile(member) as stream:
            (notices / filename).write_bytes(stream.read())
