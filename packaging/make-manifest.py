#!/usr/bin/env python3
"""Run inside the release builder after committing and building the final source."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

source, output = (Path(p).resolve() for p in sys.argv[1:])


def command(*args):
    return subprocess.check_output(args, text=True).strip()


if command("git", "-C", str(source), "status", "--porcelain"):
    raise SystemExit("Commit the final source before creating release manifests")
manifest = {
    "application": "StellarVoyager",
    "version": "1.0.0",
    "repository": "https://github.com/Stellarsue/StellarVoyager",
    "commit": command("git", "-C", str(source), "rev-parse", "HEAD"),
    "target": "Linux x86_64, glibc >= 2.35, OpenGL >= 3.3, X11/XWayland",
    "build_distribution": command("lsb_release", "-ds") if Path("/usr/bin/lsb_release").exists()
                          else "Ubuntu 22.04 (Docker)",
    "compiler": command("g++", "--version").splitlines()[0],
    "qt_version": command("qmake6", "-query", "QT_VERSION"),
    "cmake_version": command("cmake", "--version").splitlines()[0],
    "tool_sha256": {},
    "artifacts": [],
}
for name in ("linuxdeploy-x86_64.AppImage", "linuxdeploy-plugin-qt-x86_64.AppImage", "runtime-x86_64"):
    manifest["tool_sha256"][name] = hashlib.sha256((Path("/tools") / name).read_bytes()).hexdigest()
for path in sorted(output.iterdir()):
    if path.is_file() and path.name not in ("build-manifest.json", "SHA256SUMS"):
        manifest["artifacts"].append({"name": path.name, "size": path.stat().st_size,
                                     "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
(output / "build-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
checksums = []
for path in sorted(output.iterdir()):
    if path.is_file() and path.name != "SHA256SUMS":
        checksums.append(f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}\n")
(output / "SHA256SUMS").write_text("".join(checksums))
